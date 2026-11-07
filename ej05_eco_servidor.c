#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void close_socket(int descriptor)
{
    if (close(descriptor) == -1) {
        perror("close");
    }
}

static int send_all(int descriptor, const char *data, size_t length)
{
    size_t sent = 0;

    /* A successful send may transmit fewer bytes than requested. */
    while (sent < length) {
        ssize_t n = send(
            descriptor,
            data + sent,
            length - sent,
            MSG_NOSIGNAL
        );

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (n == 0) {
            errno = EIO;
            return -1;
        }

        sent += (size_t)n;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if ((argc != 2 && argc != 3)
        || (argc == 3 && strcmp(argv[2], "noreuse") != 0)) {
        fprintf(stderr, "Usage: %s <port> [noreuse]\n", argv[0]);
        return 1;
    }

    char *end = NULL;
    errno = 0;
    long port = strtol(argv[1], &end, 10);

    if (errno != 0 || end == argv[1] || *end != '\0'
        || port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port: use a number from 1 to 65535\n");
        fprintf(stderr, "Usage: %s <port> [noreuse]\n", argv[0]);
        return 1;
    }

    int server = socket(AF_INET, SOCK_STREAM, 0);

    if (server == -1) {
        perror("socket");
        return 1;
    }

    int reuse = (argc == 2);

    /* Enable address reuse unless explicitly disabled. */
    if (reuse) {
        int enabled = 1;

        if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
                       &enabled, sizeof enabled) == -1) {
            perror("setsockopt");
            close_socket(server);
            return 1;
        }
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server, (struct sockaddr *)&address, sizeof address) == -1) {
        perror("bind");
        close_socket(server);
        return 1;
    }

    if (listen(server, 5) == -1) {
        perror("listen");
        close_socket(server);
        return 1;
    }

    printf("Echo server listening on IPv4 port %ld\n", port);
    printf("SO_REUSEADDR: %s\n", reuse ? "enabled" : "disabled");

    /* Handle one client at a time. */
    for (;;) {
        struct sockaddr_in peer = {0};
        socklen_t peer_length = sizeof peer;

        int client = accept(
            server,
            (struct sockaddr *)&peer,
            &peer_length
        );

        if (client == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            break;
        }

        char ip[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof ip) == NULL) {
            perror("inet_ntop");
            close_socket(client);
            continue;
        }

        unsigned int peer_port = (unsigned int)ntohs(peer.sin_port);
        printf("Client connected: %s:%u\n", ip, peer_port);

        char buffer[4096];

        for (;;) {
            ssize_t received = recv(client, buffer, sizeof buffer, 0);

            if (received == -1) {
                if (errno == EINTR) {
                    continue;
                }

                perror("recv");
                break;
            }

            if (received == 0) {
                break;
            }

            /* Echo exactly the bytes received, without assuming text. */
            if (send_all(client, buffer, (size_t)received) == -1) {
                perror("send");
                break;
            }
        }

        close_socket(client);
        printf("Client disconnected: %s:%u\n", ip, peer_port);
    }

    close_socket(server);
    return 1;
}