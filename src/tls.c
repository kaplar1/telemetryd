/*
 * TLS wrapper — STUB. Implement with OpenSSL or mbedTLS in Stage E.
 * Left unimplemented on purpose so you write it yourself and can speak to it.
 *
 * OpenSSL sketch:
 *   SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
 *   SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
 *   SSL_CTX_use_certificate_chain_file(ctx, cert_pem);
 *   SSL_CTX_use_PrivateKey_file(ctx, key_pem, SSL_FILETYPE_PEM);
 *   SSL_CTX_load_verify_locations(ctx, ca_pem, NULL);   // for mutual TLS
 *   SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
 */

#include <sys/types.h>
#include "tls.h"

struct tls_ctx     { void *impl; };
struct tls_session { int fd; void *impl; };

tls_ctx *tls_server_init(const char *cert_pem, const char *key_pem,
                         const char *ca_pem) {
    (void)cert_pem; (void)key_pem; (void)ca_pem;
    return 0; /* TODO */
}
void tls_server_free(tls_ctx *ctx) { (void)ctx; /* TODO */ }

tls_session *tls_accept(tls_ctx *ctx, int client_fd) {
    (void)ctx; (void)client_fd; return 0; /* TODO: handshake + cert verify */
}
ssize_t tls_read (tls_session *s, void *buf, size_t len)       { (void)s;(void)buf;(void)len; return -1; }
ssize_t tls_write(tls_session *s, const void *buf, size_t len) { (void)s;(void)buf;(void)len; return -1; }
void    tls_close(tls_session *s) { (void)s; /* TODO */ }
