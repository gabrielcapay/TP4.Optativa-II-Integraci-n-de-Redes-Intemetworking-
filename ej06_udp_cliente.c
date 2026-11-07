#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if ((argc != 3 && argc != 4)
        || (argc == 4 && strcmp(argv[3], "conectado") != 0)) {
        fprintf(stderr, "Usage: %s <host> <port> [conectado]\n",
                argv[0]);
        return 1;
    }

    char *end = NULL;
    errno = 0;
    long port = strtol(argv[2], &end, 10);

    if (errno != 0 || end == argv[2] || *end != '\0'
        || port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port: use a number from 1 to 65535\n");
        fprintf(stderr, "Usage: %s <host> <port> [conectado]\n",
                argv[0]);
        return 1;
    }

    int connected = (argc == 4);

    struct addrinfo hints = {0};
    struct addrinfo *results = NULL;

    /* Match the IPv4 UDP server used in this laboratory. */
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICSERV;

    int error = getaddrinfo(argv[1], argv[2], &hints, &results);

    if (error != 0) {
        if (error == EAI_SYSTEM) {
            perror("System error");
        }

        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return 1;
    }

    int client = socket(
        results->ai_family,
        results->ai_socktype,
        results->ai_protocol
    );

    if (client == -1) {
        perror("socket");
        freeaddrinfo(results);
        return 1;
    }

    int status = 1;

    struct timeval timeout = {0};
    timeout.tv_sec = 2;

    if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof timeout) == -1) {
        perror("setsockopt");
        goto cleanup;
    }

    if (connected) {
        if (connect(client, results->ai_addr,
                    results->ai_addrlen) == -1) {
            perror("connect");
            goto cleanup;
        }
    }

    printf("UDP mode: %s\n",
           connected ? "connected" : "unconnected");
    printf("Receive timeout: 2 seconds\n");
    printf("Maximum attempts per message: 4\n");

    const size_t sizes[] = {1, 18, 1000};
    unsigned char outgoing[1000];
    unsigned char incoming[65536];

    const struct sockaddr_in *destination =
        (const struct sockaddr_in *)results->ai_addr;

    status = 0;

    for (size_t message = 0;
         message < sizeof sizes / sizeof sizes[0];
         message++) {
        size_t length = sizes[message];
        int delivered = 0;

        /* Give each message a distinct payload to detect stale replies. */
        memset(outgoing, (int)('A' + message), length);

        for (int attempt = 1; attempt <= 4; attempt++) {
            printf("Message %zu | Bytes: %zu | Attempt: %d/4\n",
                   message + 1, length, attempt);

            ssize_t sent;

            do {
                if (connected) {
                    sent = send(client, outgoing, length, 0);
                } else {
                    sent = sendto(
                        client, outgoing, length, 0,
                        results->ai_addr, results->ai_addrlen
                    );
                }
            } while (sent == -1 && errno == EINTR);

            if (sent == -1) {
                perror("send");
                break;
            }

            /* Never split a UDP message into additional sends. */
            if ((size_t)sent != length) {
                fprintf(stderr, "Unexpected datagram send size\n");
                break;
            }

            struct sockaddr_in source = {0};
            socklen_t source_length = sizeof source;

            ssize_t received;

            if (connected) {
                received = recv(client, incoming, sizeof incoming, 0);
            } else {
                received = recvfrom(
                    client, incoming, sizeof incoming, 0,
                    (struct sockaddr *)&source, &source_length
                );
            }

            if (received == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("Timeout: %s\n", strerror(errno));
                    continue;
                }

                if (errno == EINTR) {
                    fprintf(stderr, "Receive interrupted; retrying\n");
                    continue;
                }

                perror("receive");
                break;
            }

            /* Unconnected sockets must validate the reply's sender. */
            if (!connected
                && (source.sin_family != AF_INET
                    || source.sin_addr.s_addr
                       != destination->sin_addr.s_addr
                    || ntohs(source.sin_port)
                       != ntohs(destination->sin_port))) {
                fprintf(stderr, "Unexpected sender; retrying\n");
                continue;
            }

            if ((size_t)received != length
                || memcmp(incoming, outgoing, length) != 0) {
                fprintf(stderr, "Unexpected response size or content\n");
                continue;
            }

            printf("Verified response: %zd bytes, matching content\n",
                   received);

            delivered = 1;
            break;
        }

        if (!delivered) {
            printf("Message %zu abandoned\n", message + 1);
            status = 1;
        }
    }

cleanup:
    freeaddrinfo(results);

    if (close(client) == -1) {
        perror("close");
        status = 1;
    }

    return status;
}