#define _GNU_SOURCE   /* accept4() with SOCK_NONBLOCK|SOCK_CLOEXEC */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <systemd/sd-daemon.h>
#include <systemd/sd-event.h>

#include "net.h"
#include "tls.h"
#include "telemetry.h"

/* Per-connection read handler. Registered against the event loop when a new
 * client is accepted. Reads a chunk, records it, echoes it back. */
static int on_client_io(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
    (void)revents;
    struct telemetry_state *st = userdata;
    char buf[1024];

    /* TODO(TLS): when TLS is enabled, read via tls_read() instead of recv(). */
    ssize_t n = recv(fd, buf, sizeof buf, 0);
    if (n <= 0) {                        /* peer closed or error */
        if (st->active_connections) st->active_connections--;
        sd_event_source_unref(s);        /* stops watching this fd */
        close(fd);
        return 0;
    }

    telemetry_record_message(st, (size_t)n);
    send(fd, buf, (size_t)n, 0);         /* echo; replace with real protocol */
    return 0;
}

/* Accept handler on the listening socket. */
static int on_accept(sd_event_source *s, int listen_fd, uint32_t revents, void *userdata) {
    (void)revents;
    struct telemetry_state *st = userdata;
    struct sockaddr_in peer; socklen_t plen = sizeof peer;

    int cfd = accept4(listen_fd, (struct sockaddr *)&peer, &plen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (cfd < 0) return 0;

    /* TODO(TLS): tls_accept(st->tls_ctx, cfd) -> per-connection TLS session,
     * validate the client certificate, then attach the session below. */

    st->active_connections++;
    sd_event *e = sd_event_source_get_event(s);
    sd_event_add_io(e, NULL, cfd, EPOLLIN, on_client_io, st);
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

    /* TODO(TLS): st->tls_ctx = tls_server_init("/etc/telemetryd/cert.pem", ...) */

    st->listen_fd = fd;
    return sd_event_add_io(event, NULL, fd, EPOLLIN, on_accept, st);
}

void net_listen_cleanup(struct telemetry_state *st) {
    if (st && st->listen_fd > 0) close(st->listen_fd);
    /* TODO(TLS): tls_server_free(st->tls_ctx); */
}
