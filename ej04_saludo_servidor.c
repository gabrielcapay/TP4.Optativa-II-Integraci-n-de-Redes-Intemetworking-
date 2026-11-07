#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    /* Validate the complete argument before converting to 16 bits. */
    char *end = NULL;
    errno = 0;
    long port = strtol(argv[1], &end, 10);

    if (errno != 0 || end == argv[1] || *end != '\0'
        || port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port: use a number from 1 to 65535\n");
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int server = socket(AF_INET, SOCK_STREAM, 0);

    if (server == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);

    /* Bind to all local IPv4 interfaces. */
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    int status = 0;

    if (bind(server, (struct sockaddr *)&address, sizeof address) == -1) {
        perror("bind");
        status = 1;
    } else if (listen(server, 5) == -1) {
        perror("listen");
        status = 1;
    } else {
        printf("Listening on IPv4 port %ld\n", port);

        /* Keep the listening socket open between clients. */
        for (;;) {
            struct sockaddr_in client_address = {0};
            socklen_t client_length = sizeof client_address;

            int client = accept(
                server,
                (struct sockaddr *)&client_address,
                &client_length
            );

            if (client == -1) {
                if (errno == EINTR) {
                    continue;
                }

                perror("accept");
                status = 1;
                break;
            }

            char ip[INET_ADDRSTRLEN];

            if (inet_ntop(AF_INET, &client_address.sin_addr,
                          ip, sizeof ip) == NULL) {
                perror("inet_ntop");
                status = 1;
            } else {
                unsigned int client_port =
                    (unsigned int)ntohs(client_address.sin_port);

                printf("Client connected: %s:%u\n", ip, client_port);

                char message[128];
                int length = snprintf(
                    message, sizeof message,
                    "Hola, te conectaste desde %s:%u\n",
                    ip, client_port
                );

                if (length < 0 || (size_t)length >= sizeof message) {
                    fprintf(stderr, "Could not format greeting\n");
                    status = 1;
                } else {
                    size_t sent = 0;

                    /* Send the entire greeting, handling partial sends. */
                    while (sent < (size_t)length) {
                        ssize_t n = send(
                            client,
                            message + sent,
                            (size_t)length - sent,
                            MSG_NOSIGNAL
                        );

                        if (n == -1) {
                            if (errno == EINTR) {
                                continue;
                            }

                            perror("send");
                            status = 1;
                            break;
                        }

                        if (n == 0) {
                            fprintf(stderr, "send made no progress\n");
                            status = 1;
                            break;
                        }

                        sent += (size_t)n;
                    }
                }
            }

            /* Close only the connection belonging to this client. */
            if (close(client) == -1) {
                perror("close client");
                status = 1;
            }
        }
    }

    /* Release the listening socket if setup or accept fails. */
    if (close(server) == -1) {
        perror("close server");
        status = 1;
    }

    return status;
}