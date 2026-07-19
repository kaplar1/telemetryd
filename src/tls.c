/*
 * TLS wrapper around OpenSSL. Mutual TLS by design: telemetryd only accepts
 * clients that present a certificate chaining to our pinned CA, in addition
 * to the client verifying our server certificate. That fits an embedded
 * telemetry service better than server-only TLS -- we want to know which
 * device is talking to us, not just that the channel is encrypted.
 *
 * net.c owns the socket and the event loop; this file only ever touches
 * the fd it's handed, never closes it itself (see tls_close()).
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "tls.h"

/* Bound how long a single connection's handshake may occupy the event loop.
 * telemetryd is single-threaded (see telemetry.h); a client that opens the
 * TCP connection and then stalls the TLS handshake would otherwise block
 * every other client's I/O indefinitely. This turns that into a bounded
 * stall instead of an unbounded one -- worth a line in THREAT_MODEL.md as
 * a known DoS surface (a determined attacker can still tie up the loop for
 * up to this many seconds per connection; a production deployment facing
 * untrusted networks would want a worker pool or per-conn async handshake
 * instead of the synchronous-with-timeout approach used here). */
#define TLS_HANDSHAKE_TIMEOUT_SEC 5

struct tls_ctx     { SSL_CTX *ssl_ctx; };
struct tls_session { int fd; SSL *ssl; };

static void log_openssl_errors(const char *what) {
    unsigned long e;
    while ((e = ERR_get_error()) != 0) {
        char buf[256];
        ERR_error_string_n(e, buf, sizeof buf);
        fprintf(stderr, "tls: %s: %s\n", what, buf);
    }
}

tls_ctx *tls_server_init(const char *cert_pem, const char *key_pem,
                          const char *ca_pem) {
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) { log_openssl_errors("SSL_CTX_new"); return NULL; }

    /* Floor at TLS 1.2 -- reject SSLv3/TLS1.0/TLS1.1 downgrade attempts. */
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1) {
        log_openssl_errors("SSL_CTX_set_min_proto_version");
        goto fail;
    }

    /* AEAD-only cipher list for the TLS 1.2 case (TLS 1.3, if negotiated,
     * is AEAD-only by construction in OpenSSL and controlled separately
     * via SSL_CTX_set_ciphersuites -- the defaults there are already fine
     * so we leave them alone). Forward-secret (ECDHE) key exchange only. */
    if (SSL_CTX_set_cipher_list(ctx,
            "ECDHE-ECDSA-AES128-GCM-SHA256:"
            "ECDHE-RSA-AES128-GCM-SHA256:"
            "ECDHE-ECDSA-AES256-GCM-SHA384:"
            "ECDHE-RSA-AES256-GCM-SHA384") != 1) {
        log_openssl_errors("SSL_CTX_set_cipher_list");
        goto fail;
    }
    SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

    /* No session resumption cache: keeps the security model simple (every
     * connection does a full handshake and a full peer-cert check) at the
     * cost of a little CPU. Fine at telemetry-daemon connection volumes. */
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

    if (SSL_CTX_use_certificate_chain_file(ctx, cert_pem) != 1) {
        fprintf(stderr, "tls: failed to load server cert '%s'\n", cert_pem);
        log_openssl_errors("SSL_CTX_use_certificate_chain_file");
        goto fail;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, key_pem, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "tls: failed to load server key '%s'\n", key_pem);
        log_openssl_errors("SSL_CTX_use_PrivateKey_file");
        goto fail;
    }
    if (SSL_CTX_check_private_key(ctx) != 1) {
        fprintf(stderr, "tls: server cert/key mismatch\n");
        log_openssl_errors("SSL_CTX_check_private_key");
        goto fail;
    }

    if (ca_pem) {
        /* Mutual TLS: pin the CA we trust for *client* certs, require one
         * on every connection, and fail closed if it doesn't chain to it
         * or the client doesn't present one at all. */
        if (SSL_CTX_load_verify_locations(ctx, ca_pem, NULL) != 1) {
            fprintf(stderr, "tls: failed to load CA '%s'\n", ca_pem);
            log_openssl_errors("SSL_CTX_load_verify_locations");
            goto fail;
        }
        STACK_OF(X509_NAME) *names = SSL_load_client_CA_file(ca_pem);
        if (!names) {
            log_openssl_errors("SSL_load_client_CA_file");
            goto fail;
        }
        SSL_CTX_set_client_CA_list(ctx, names);
        SSL_CTX_set_verify(ctx,
            SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
        SSL_CTX_set_verify_depth(ctx, 2); /* leaf + issuing CA, no deeper */
    }

    tls_ctx *out = calloc(1, sizeof *out);
    if (!out) { fprintf(stderr, "tls: out of memory\n"); goto fail; }
    out->ssl_ctx = ctx;
    return out;

fail:
    SSL_CTX_free(ctx);
    return NULL;
}

void tls_server_free(tls_ctx *ctx) {
    if (!ctx) return;
    /* SSL_CTX_free() drops our reference to the internal EVP_PKEY holding
     * the private key; OpenSSL clears the sensitive BIGNUM material for
     * RSA/EC keys as part of that teardown (OPENSSL_clear_free internally)
     * on modern OpenSSL (>=1.1.0), which is as close to "zeroise on
     * shutdown" as we get without hand-parsing the key file ourselves --
     * we never see the raw key bytes in our own buffers to begin with,
     * since SSL_CTX_use_PrivateKey_file() does the loading internally. */
    SSL_CTX_free(ctx->ssl_ctx);
    free(ctx);
}

/* Handshake with a bounded wall-clock timeout. tls_accept()'s contract
 * (tls.h) is "does the handshake" -- i.e. synchronous from the caller's
 * point of view -- even though client_fd is O_NONBLOCK (net.c hands us an
 * accept4(..., SOCK_NONBLOCK) fd). We honor that by polling on whichever
 * direction OpenSSL says it's blocked on and retrying, rather than
 * requiring net.c to drive a handshake state machine through the event
 * loop. Simpler call contract, at the cost of the DoS exposure noted on
 * TLS_HANDSHAKE_TIMEOUT_SEC above. */
tls_session *tls_accept(tls_ctx *ctx, int client_fd) {
    if (!ctx) { fprintf(stderr, "tls: tls_accept called with no context\n"); return NULL; }

    SSL *ssl = SSL_new(ctx->ssl_ctx);
    if (!ssl) { log_openssl_errors("SSL_new"); return NULL; }
    if (SSL_set_fd(ssl, client_fd) != 1) {
        log_openssl_errors("SSL_set_fd");
        SSL_free(ssl);
        return NULL;
    }

    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += TLS_HANDSHAKE_TIMEOUT_SEC;

    for (;;) {
        int r = SSL_accept(ssl);
        if (r == 1) break; /* handshake complete, peer cert (if any) verified */

        int err = SSL_get_error(ssl, r);
        short want;
        if (err == SSL_ERROR_WANT_READ)       want = POLLIN;
        else if (err == SSL_ERROR_WANT_WRITE) want = POLLOUT;
        else {
            fprintf(stderr, "tls: handshake failed on fd %d\n", client_fd);
            log_openssl_errors("SSL_accept");
            SSL_free(ssl);
            return NULL;
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long ms_left = (deadline.tv_sec  - now.tv_sec)  * 1000L
                      + (deadline.tv_nsec - now.tv_nsec) / 1000000L;
        if (ms_left <= 0) {
            fprintf(stderr, "tls: handshake timed out on fd %d\n", client_fd);
            SSL_free(ssl);
            return NULL;
        }

        struct pollfd pfd = { .fd = client_fd, .events = want };
        int pr = poll(&pfd, 1, (int)ms_left);
        if (pr <= 0) { /* timeout, or a real poll() error either way we give up */
            if (pr < 0) perror("tls: poll");
            else fprintf(stderr, "tls: handshake timed out on fd %d\n", client_fd);
            SSL_free(ssl);
            return NULL;
        }
        /* fd is ready in the direction OpenSSL wanted; loop back and retry. */
    }

    /* Defensive double-check: SSL_VERIFY_FAIL_IF_NO_PEER_CERT already
     * refuses connections with no client cert when mutual TLS is on, but
     * confirm the chain actually validated too before trusting the peer. */
    if (SSL_CTX_get_verify_mode(ctx->ssl_ctx) & SSL_VERIFY_PEER) {
        long vr = SSL_get_verify_result(ssl);
        if (vr != X509_V_OK) {
            fprintf(stderr, "tls: peer cert verify failed on fd %d: %s\n",
                    client_fd, X509_verify_cert_error_string(vr));
            SSL_free(ssl);
            return NULL;
        }
    }

    tls_session *s = calloc(1, sizeof *s);
    if (!s) { fprintf(stderr, "tls: out of memory\n"); SSL_free(ssl); return NULL; }
    s->fd  = client_fd;
    s->ssl = ssl;
    return s;
}

ssize_t tls_read(tls_session *s, void *buf, size_t len) {
    if (!s) { errno = EINVAL; return -1; }

    int n = SSL_read(s->ssl, buf, (int)len);
    if (n > 0) return n;

    int err = SSL_get_error(s->ssl, n);
    switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        /* Not enough bytes yet for a full TLS record (WANT_READ), or,
         * rarely, a mid-stream renegotiation-style operation needing a
         * write (WANT_WRITE). Neither means the connection is dead --
         * the caller must treat this like recv()'s EAGAIN/EWOULDBLOCK,
         * not like EOF. */
        errno = EAGAIN;
        return -1;
    case SSL_ERROR_ZERO_RETURN:
        /* Peer sent a clean TLS close_notify. */
        return 0;
    default:
        log_openssl_errors("SSL_read");
        errno = EIO;
        return -1;
    }
}

ssize_t tls_write(tls_session *s, const void *buf, size_t len) {
    if (!s) { errno = EINVAL; return -1; }

    int n = SSL_write(s->ssl, buf, (int)len);
    if (n > 0) return n;

    int err = SSL_get_error(s->ssl, n);
    switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        errno = EAGAIN;
        return -1;
    default:
        log_openssl_errors("SSL_write");
        errno = EIO;
        return -1;
    }
}

void tls_close(tls_session *s) {
    if (!s) return;
    if (s->ssl) {
        /* Best-effort close_notify; we don't loop waiting for the peer's
         * own close_notify back (SSL_shutdown's "unidirectional" case) --
         * the fd is getting close()'d by net.c right after this either
         * way, so a strict bidirectional shutdown wouldn't buy anything
         * here. */
        SSL_shutdown(s->ssl);
        SSL_free(s->ssl);
    }
    free(s);
}
