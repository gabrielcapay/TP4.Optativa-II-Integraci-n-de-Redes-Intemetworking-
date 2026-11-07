#define _POSIX_C_SOURCE 200112L

#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <service>\n", argv[0]);
        return 1;
    }

    struct addrinfo hints = {0};
    struct addrinfo *results = NULL;

    /* Request stream socket addresses for both IPv4 and IPv6. */
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

    size_t count = 0;
    int status = 0;

    /* Traverse every address returned by the resolver. */
    for (struct addrinfo *current = results;
         current != NULL;
         current = current->ai_next) {
        count++;

        char ip[128];
        char service[32];

        /* Obtain the numeric IP address and port as text. */
        int name_error = getnameinfo(
            current->ai_addr, current->ai_addrlen,
            ip, sizeof ip,
            service, sizeof service,
            NI_NUMERICHOST | NI_NUMERICSERV
        );

        if (name_error != 0) {
            if (name_error == EAI_SYSTEM) {
                perror("System error");
            }

            fprintf(stderr, "getnameinfo: %s\n",
                    gai_strerror(name_error));
            status = 1;
            continue;
        }

        const char *family = "Other";

        if (current->ai_family == AF_INET) {
            family = "IPv4";
        } else if (current->ai_family == AF_INET6) {
            family = "IPv6";
        }

        printf("%s | IP: %s | Port: %s\n", family, ip, service);

        char reverse_name[1025];

        /* Require a name instead of falling back to a numeric IP. */
        int reverse_error = getnameinfo(
            current->ai_addr, current->ai_addrlen,
            reverse_name, sizeof reverse_name,
            NULL, 0,
            NI_NAMEREQD
        );

        if (reverse_error == 0) {
            printf("Reverse name: %s\n", reverse_name);
        } else if (reverse_error == EAI_NONAME) {
            printf("Reverse name: not found\n");
        } else {
            if (reverse_error == EAI_SYSTEM) {
                perror("System error");
            }

            fprintf(stderr, "Reverse lookup: %s\n",
                    gai_strerror(reverse_error));
            status = 1;
        }
    }

    printf("Results for %s / %s: %zu\n", argv[1], argv[2], count);

    /* Release the entire result list, including after lookup errors. */
    freeaddrinfo(results);

    return status;
}