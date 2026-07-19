/*
 * Test client for telemetryd, speaking the same protocol net.c does: raw
 * bytes over TLS, echoed back by the server -- no framing.
 *
 * Usage: client <host> <port> <message> <client.crt> <client.key> <ca.crt>
 *   e.g. ./tools/client 127.0.0.1 9099 "hello telemetryd" \
 *            certs/client.crt certs/client.key certs/ca.crt
 *
 * Mutual TLS: telemetryd requires a client certificate chaining to its
 * pinned CA (see tls.c), so this client always presents one -- there's no
 * plaintext fallback, matching the server's fail-closed design.
 */

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

static void print_ssl_errors(const char *what) {
    unsigned long e;
    while ((e = ERR_get_error()) != 0) {
        char buf[256];
        ERR_error_string_n(e, buf, sizeof buf);
        fprintf(stderr, "%s: %s\n", what, buf);
    }
}

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr,
            "usage: %s <host> <port> <message> <client.crt> <client.key> <ca.crt>\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    const char *host       = argv[1];
    uint16_t    port       = (uint16_t)atoi(argv[2]);
    const char *msg        = argv[3];
    const char *client_crt = argv[4];
    const char *client_key = argv[5];
    const char *ca_crt     = argv[6];
    size_t msg_len = strlen(msg);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid host: %s\n", host);
        close(fd);
        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { print_ssl_errors("SSL_CTX_new"); close(fd); return EXIT_FAILURE; }

    if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1 ||
        SSL_CTX_load_verify_locations(ctx, ca_crt, NULL) != 1 ||
        SSL_CTX_use_certificate_file(ctx, client_crt, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_use_PrivateKey_file(ctx, client_key, SSL_FILETYPE_PEM) != 1) {
        print_ssl_errors("SSL_CTX setup");
        SSL_CTX_free(ctx);
        close(fd);
        return EXIT_FAILURE;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL); /* verify the server's cert */

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    SSL_set_tlsext_host_name(ssl, host); /* SNI, harmless even without vhosting */

    if (SSL_connect(ssl) != 1) {
        fprintf(stderr, "TLS handshake failed\n");
        print_ssl_errors("SSL_connect");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return EXIT_FAILURE;
    }
    printf("TLS handshake OK: %s, cipher %s\n",
           SSL_get_version(ssl), SSL_get_cipher(ssl));

    if (SSL_write(ssl, msg, (int)msg_len) <= 0) {
        print_ssl_errors("SSL_write");
        goto cleanup;
    }
    printf("sent %zu bytes to %s:%u over TLS: \"%s\"\n", msg_len, host, port, msg);

    char buf[1024];
    int n = SSL_read(ssl, buf, sizeof(buf) - 1);
    if (n <= 0) {
        print_ssl_errors("SSL_read");
        goto cleanup;
    }
    buf[n] = '\0';
    printf("received %d bytes echoed back: \"%s\"\n", n, buf);

cleanup:
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(fd);
    return EXIT_SUCCESS;
}
