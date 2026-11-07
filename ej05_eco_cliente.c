#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static int send_all(int descriptor, const char *data, size_t length)
{
    size_t sent = 0;

    /* Continue until every byte has been sent. */
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
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <service>\n", argv[0]);
        return 1;
    }

    struct addrinfo hints = {0};
    struct addrinfo *results = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int error = getaddrinfo(argv[1], argv[2], &hints, &results);

    if (error != 0) {
        if (error == EAI_SYSTEM) {
            perror("System error");
        }

        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return 1;
    }

    int client = -1;
    size_t attempt = 0;

    /* Try every returned address until a connection succeeds. */
    for (struct addrinfo *current = results;
         current != NULL;
         current = current->ai_next) {
        attempt++;

        const char *family = "Other";

        if (current->ai_family == AF_INET) {
            family = "IPv4";
        } else if (current->ai_family == AF_INET6) {
            family = "IPv6";
        }

        printf("Connection attempt %zu (%s)\n", attempt, family);

        client = socket(
            current->ai_family,
            current->ai_socktype,
            current->ai_protocol
        );

        if (client == -1) {
            perror("socket");
            continue;
        }

        if (connect(client, current->ai_addr,
                    current->ai_addrlen) == 0) {
            printf("Connection successful\n");
            break;
        }

        perror("connect");

        if (close(client) == -1) {
            perror("close");
        }

        client = -1;
    }

    freeaddrinfo(results);

    if (client == -1) {
        fprintf(stderr, "Could not connect to any address\n");
        return 1;
    }

    printf("Enter text. Press Ctrl+D on an empty line to finish.\n");

    char *line = NULL;
    size_t capacity = 0;
    int status = 0;

    for (;;) {
        ssize_t length = getline(&line, &capacity, stdin);

        if (length == -1) {
            if (ferror(stdin)) {
                perror("getline");
                status = 1;
            }

            break;
        }

        if (send_all(client, line, (size_t)length) == -1) {
            perror("send");
            status = 1;
            break;
        }

        size_t remaining = (size_t)length;
        char buffer[4096];

        /* TCP may deliver the echoed line across several reads. */
        while (remaining > 0) {
            size_t requested = remaining;

            if (requested > sizeof buffer) {
                requested = sizeof buffer;
            }

            ssize_t received = recv(client, buffer, requested, 0);

            if (received == -1) {
                if (errno == EINTR) {
                    continue;
                }

                perror("recv");
                status = 1;
                goto cleanup;
            }

            if (received == 0) {
                fprintf(stderr, "Server closed before the complete echo\n");
                status = 1;
                goto cleanup;
            }

            if (fwrite(buffer, 1, (size_t)received, stdout)
                != (size_t)received) {
                perror("fwrite");
                status = 1;
                goto cleanup;
            }

            remaining -= (size_t)received;
        }

        if (fflush(stdout) == EOF) {
            perror("fflush");
            status = 1;
            break;
        }
    }

cleanup:
    free(line);

    if (close(client) == -1) {
        perror("close");
        status = 1;
    }

    return status;
}