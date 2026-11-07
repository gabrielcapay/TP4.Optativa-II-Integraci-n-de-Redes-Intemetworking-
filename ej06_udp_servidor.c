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

    char *end = NULL;
    errno = 0;
    long port = strtol(argv[1], &end, 10);

    if (errno != 0 || end == argv[1] || *end != '\0'
        || port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port: use a number from 1 to 65535\n");
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int server = socket(AF_INET, SOCK_DGRAM, 0);

    if (server == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    int status = 0;

    if (bind(server, (struct sockaddr *)&address, sizeof address) == -1) {
        perror("bind");
        status = 1;
    } else {
        printf("UDP echo server on IPv4 port %ld\n", port);

        /* Large enough for any IPv4 UDP payload. */
        char buffer[65536];

        for (;;) {
            struct sockaddr_in peer = {0};
            socklen_t peer_length = sizeof peer;

            ssize_t received = recvfrom(
                server,
                buffer,
                sizeof buffer,
                0,
                (struct sockaddr *)&peer,
                &peer_length
            );

            if (received == -1) {
                if (errno == EINTR) {
                    continue;
                }

                perror("recvfrom");
                status = 1;
                break;
            }

            char ip[INET_ADDRSTRLEN];

            if (inet_ntop(AF_INET, &peer.sin_addr,
                          ip, sizeof ip) == NULL) {
                perror("inet_ntop");
                status = 1;
                continue;
            }

            unsigned int peer_port = (unsigned int)ntohs(peer.sin_port);

            printf("Datagram from %s:%u | Bytes: %zd\n",
                   ip, peer_port, received);

            ssize_t sent;

            /* Retry only if interrupted before sending the datagram. */
            do {
                sent = sendto(
                    server,
                    buffer,
                    (size_t)received,
                    0,
                    (struct sockaddr *)&peer,
                    peer_length
                );
            } while (sent == -1 && errno == EINTR);

            if (sent == -1) {
                perror("sendto");
                status = 1;
            } else if (sent != received) {
                fprintf(stderr, "Unexpected datagram send size\n");
                status = 1;
            }
        }
    }

    if (close(server) == -1) {
        perror("close");
        status = 1;
    }

    return status;
}