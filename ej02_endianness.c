#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(void)
{
    uint16_t value = 0x0102;

    /* Inspect the object's bytes in increasing memory addresses. */
    const unsigned char *bytes = (const unsigned char *)&value;

    printf("Bytes in memory: %02X %02X\n",
           (unsigned int)bytes[0],
           (unsigned int)bytes[1]);

    if (bytes[0] == 0x02 && bytes[1] == 0x01) {
        printf("Host byte order: little-endian\n");
    } else if (bytes[0] == 0x01 && bytes[1] == 0x02) {
        printf("Host byte order: big-endian\n");
    } else {
        fprintf(stderr, "Unexpected byte order\n");
        return 1;
    }

    uint16_t port = 8080;
    uint16_t network_port = htons(port);

    /* Compare the bytes before and after conversion. */
    const unsigned char *host_bytes = (const unsigned char *)&port;
    const unsigned char *network_bytes =
        (const unsigned char *)&network_port;

    printf("8080 before htons: %02X %02X\n",
           (unsigned int)host_bytes[0],
           (unsigned int)host_bytes[1]);

    printf("8080 after htons:  %02X %02X\n",
           (unsigned int)network_bytes[0],
           (unsigned int)network_bytes[1]);

        uint32_t ip = 0xC0A8010A;
    uint32_t network_ip = htonl(ip);

    const unsigned char *host_ip_bytes = (const unsigned char *)&ip;
    const unsigned char *network_ip_bytes =
        (const unsigned char *)&network_ip;

    /* Display each byte in increasing memory addresses. */
    printf("IP before htonl:");
    for (size_t i = 0; i < sizeof ip; i++) {
        printf(" %02X", (unsigned int)host_ip_bytes[i]);
    }
    printf("\n");

    printf("IP after htonl: ");
    for (size_t i = 0; i < sizeof network_ip; i++) {
        printf(" %02X", (unsigned int)network_ip_bytes[i]);
    }
    printf("\n");

        struct sockaddr_in address = {0};
    address.sin_family = AF_INET;

    /* Deliberately omit conversion to demonstrate the error. */
    address.sin_port = 8080;
    printf("Port without htons, read with ntohs: %u\n",
           (unsigned int)ntohs(address.sin_port));

    /* Store the port correctly in network byte order. */
    address.sin_port = htons(8080);
    printf("Port with htons, read with ntohs:    %u\n",
           (unsigned int)ntohs(address.sin_port));
           
    return 0;
}