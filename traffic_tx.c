
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pcap/pcap.h>
#include <stdint.h>

#define INTERFACE "wlan1"

int main()
{
    pcap_t *handle;
    char errbuf[PCAP_ERRBUF_SIZE];

    uint8_t packet[1500] = {0};

    /* Radiotap header */
    packet[2] = 8;

    /* 802.11 data frame */
    packet[8] = 0x08;
    packet[9] = 0x00;

    /* Destination address */
    uint8_t dest[6] = {0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff};

    /* Source address */
    uint8_t src[6] = {0x02, 0x11, 0x22,
                      0x33, 0x44, 0x55};

    memcpy(&packet[12], dest, 6);
    memcpy(&packet[18], src, 6);
    memcpy(&packet[24], src, 6);

    memcpy(&packet[32], "Hello SoftMAC TX", 17);

    handle = pcap_open_live(INTERFACE, 65535, 1, 1000, errbuf);

    if (!handle) {
        printf("Error: %s\n", errbuf);
        return 1;
    }

    printf("TX traffic started on %s\n", INTERFACE);

    while (1) {
        if (pcap_inject(handle, packet, sizeof(packet)) > 0)
         {}
           // printf("TX packet sent: %zu bytes\n", sizeof(packet));

    }

    pcap_close(handle);
    return 0;
}
