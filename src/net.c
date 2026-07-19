#define _GNU_SOURCE   /* accept4() with SOCK_NONBLOCK|SOCK_CLOEXEC */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <systemd/sd-daemon.h>
#include <systemd/sd-event.h>

#include "net.h"
#include "tls.h"
#include "telemetry.h"

/* Where the server cert/key/CA live on target (FHS-ish path under
 * /etc/telemetryd; a Yocto recipe or provisioning step is responsible for
 * getting real files there with the right perms -- 0600, owned by the
 * telemetryd service user, per tls.h's checklist). Overridable via env
 * vars so local/native testing doesn't need root-owned /etc paths. */
#define TLS_CERT_ENV "TELEMETRYD_TLS_CERT"
#define TLS_KEY_ENV  "TELEMETRYD_TLS_KEY"
#define TLS_CA_ENV   "TELEMETRYD_TLS_CA"
#define TLS_CERT_DEFAULT "/etc/telemetryd/tls/server.crt"
#define TLS_KEY_DEFAULT  "/etc/telemetryd/tls/server.key"
#define TLS_CA_DEFAULT   "/etc/telemetryd/tls/ca.crt"

/* Per-connection state. sd_event_add_io()'s userdata is per-registration,
 * so each client's TLS session needs its own little struct rather than
 * sharing the global telemetry_state directly the way the pre-TLS stub
 * did (it only ever needed the fd, which sd-event already hands the
 * callback separately). */
struct client_ctx {
    struct telemetry_state *st;
    tls_session             *tls;
};

/* Per-connection read handler. Registered against the event loop when a new
 * client is accepted. Reads a chunk, records it, echoes it back. */
static int on_client_io(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
    (void)revents;
    struct client_ctx *cc = userdata;
    struct telemetry_state *st = cc->st;
    char buf[1024];

    ssize_t n = tls_read(cc->tls, buf, sizeof buf);
    if (n < 0) {
        if (errno == EAGAIN) return 0;   /* partial TLS record, try again later */
        goto close_conn;                 /* real error */
    }
    if (n == 0) goto close_conn;         /* peer closed (TCP EOF or TLS close_notify) */

    telemetry_record_message(st, (size_t)n);
    /* Echo; replace with real protocol. Single tls_write() call assumes the
     * echo fits in one TLS record and one syscall -- fine for this size
     * buffer, but a production version would need an outgoing buffer and
     * to re-arm on EPOLLOUT if tls_write() ever returns EAGAIN here. */
    tls_write(cc->tls, buf, (size_t)n);
    return 0;

close_conn:
    if (st->active_connections) st->active_connections--;
    sd_event_source_unref(s);            /* stops watching this fd */
    tls_close(cc->tls);
    close(fd);
    free(cc);
    return 0;
}

/* Accept handler on the listening socket. */
static int on_accept(sd_event_source *s, int listen_fd, uint32_t revents, void *userdata) {
    (void)revents;
    struct telemetry_state *st = userdata;
    struct sockaddr_in peer; socklen_t plen = sizeof peer;

    int cfd = accept4(listen_fd, (struct sockaddr *)&peer, &plen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (cfd < 0) return 0;

    tls_session *tls = tls_accept((tls_ctx *)st->tls_ctx, cfd);
    if (!tls) {
        /* Handshake failed, timed out, or client cert didn't verify --
         * tls_accept() already logged why. Fail closed. */
        close(cfd);
        return 0;
    }

    struct client_ctx *cc = calloc(1, sizeof *cc);
    if (!cc) {
        fprintf(stderr, "net: out of memory accepting connection\n");
        tls_close(tls);
        close(cfd);
        return 0;
    }
    cc->st  = st;
    cc->tls = tls;

    st->active_connections++;
    sd_event *e = sd_event_source_get_event(s);
    sd_event_add_io(e, NULL, cfd, EPOLLIN, on_client_io, cc);
    return 0;
}

int net_listen_init(sd_event *event, struct telemetry_state *st) {
    int fd = -1;

    /* Socket activation: did systemd hand us a ready-made listening socket? */
    int n = sd_listen_fds(0);
    if (n > 1) return -EINVAL;
    if (n == 1) {
        fd = SD_LISTEN_FDS_START;                 /* the passed-in fd */
    } else {
        /* Standalone fallback: create + bind ourselves. */
        fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) return -errno;
        int one = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        struct sockaddr_in a = { .sin_family = AF_INET,
                                 .sin_addr.s_addr = htonl(INADDR_ANY),
                                 .sin_port = htons(TELEMETRYD_PORT) };
        if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0) { close(fd); return -errno; }
        if (listen(fd, SOMAXCONN) < 0) { close(fd); return -errno; }
    }

    const char *cert = getenv(TLS_CERT_ENV);
    const char *key  = getenv(TLS_KEY_ENV);
    const char *ca   = getenv(TLS_CA_ENV);
    if (!cert) cert = TLS_CERT_DEFAULT;
    if (!key)  key  = TLS_KEY_DEFAULT;
    if (!ca)   ca   = TLS_CA_DEFAULT;

    st->tls_ctx = tls_server_init(cert, key, ca);
    if (!st->tls_ctx) {
        fprintf(stderr, "net: TLS init failed (cert=%s key=%s ca=%s); refusing to "
                         "start without TLS -- this is a *secure* telemetry "
                         "daemon, not an optionally-secure one\n", cert, key, ca);
        close(fd);
        return -EINVAL;
    }

    st->listen_fd = fd;
    return sd_event_add_io(event, NULL, fd, EPOLLIN, on_accept, st);
}

void net_listen_cleanup(struct telemetry_state *st) {
    if (!st) return;
    if (st->listen_fd > 0) close(st->listen_fd);
    tls_server_free((tls_ctx *)st->tls_ctx);
}
