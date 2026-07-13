/*
 * Minimal test client for telemetryd Stage A (matches net.c's actual
 * protocol: raw bytes over TCP, echoed back by the server — no framing).
 *
 * Usage: client <host> <port> <message>
 *   e.g. ./tools/client 127.0.0.1 9099 "hello telemetryd"
 *
 * Sends the message, then reads and prints whatever the server echoes back.
 */

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <host> <port> <message>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *host = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);
    const char *msg = argv[3];
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

    if (write(fd, msg, msg_len) != (ssize_t)msg_len) {
        perror("write");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("sent %zu bytes to %s:%u: \"%s\"\n", msg_len, host, port, msg);

    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        close(fd);
        return EXIT_FAILURE;
    }
    buf[n] = '\0';
    printf("received %zd bytes echoed back: \"%s\"\n", n, buf);

    close(fd);
    return EXIT_SUCCESS;
}
