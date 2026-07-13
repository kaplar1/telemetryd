#ifndef TLS_H
#define TLS_H

#include <stddef.h>

/* Thin TLS wrapper. Implement with OpenSSL or (more typical on embedded)
 * mbedTLS. Kept behind this interface so main/net code stays transport-agnostic
 * and learn about *why* you isolated crypto here.
 *
 * Security checklist to implement (and mention in your threat model):
 *   - TLS 1.2+ only, strong cipher suites
 *   - verify the peer certificate chain against a pinned CA
 *   - reject expired / hostname-mismatched certs
 *   - load private key from a file with 0600 perms, owned by the service user
 *   - zeroise key material on shutdown
 */

typedef struct tls_ctx     tls_ctx;      /* server-wide config          */
typedef struct tls_session tls_session;  /* per-connection state        */

tls_ctx     *tls_server_init(const char *cert_pem, const char *key_pem,
                             const char *ca_pem);
void         tls_server_free(tls_ctx *ctx);

tls_session *tls_accept(tls_ctx *ctx, int client_fd);   /* does handshake */
ssize_t      tls_read (tls_session *s, void *buf, size_t len);
ssize_t      tls_write(tls_session *s, const void *buf, size_t len);
void         tls_close(tls_session *s);

#endif /* TLS_H */
