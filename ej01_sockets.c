#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    const int families[] = {AF_INET, AF_INET, AF_INET6};
    const int types[] = {SOCK_STREAM, SOCK_DGRAM, SOCK_STREAM};
    const char *names[] = {"TCP IPv4", "UDP IPv4", "TCP IPv6"};
    int descriptors[3];
    int status = 0;

    /* Keep all three sockets open until creation is complete. */
    for (int i = 0; i < 3; i++) {
        descriptors[i] = socket(families[i], types[i], 0);

        if (descriptors[i] == -1) {
            perror(names[i]);
            status = 1;
        } else {
            printf("%s descriptor: %d\n", names[i], descriptors[i]);
        }
    }

        /* Release the first descriptor and create a replacement socket. */
    if (descriptors[0] != -1) {
        if (close(descriptors[0]) == -1) {
            perror("close first socket");
            status = 1;
        }

        descriptors[0] = socket(AF_INET, SOCK_STREAM, 0);

        if (descriptors[0] == -1) {
            perror("replacement socket");
            status = 1;
        } else {
            printf("Replacement descriptor: %d\n", descriptors[0]);
        }
    }

    /* Close only sockets that were successfully created. */
    for (int i = 0; i < 3; i++) {
        if (descriptors[i] != -1) {
            if (close(descriptors[i]) == -1) {
                perror("close");
                status = 1;
            }
        }
    }

    /* Intentionally request an incompatible type and protocol. */
    int invalid = socket(AF_INET, SOCK_STREAM, IPPROTO_UDP);

    if (invalid == -1) {
        perror("SOCK_STREAM + IPPROTO_UDP");
    } else {
        fprintf(stderr, "Unexpected success for incompatible combination\n");
        status = 1;

        if (close(invalid) == -1) {
            perror("close");
        }
    }

        /* Test the reverse incompatible combination. */
    int invalid_datagram = socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP);

    if (invalid_datagram == -1) {
        perror("SOCK_DGRAM + IPPROTO_TCP");
    } else {
        fprintf(stderr, "Unexpected success for incompatible combination\n");
        status = 1;

        if (close(invalid_datagram) == -1) {
            perror("close");
            status = 1;
        }
    }

    return status;
}