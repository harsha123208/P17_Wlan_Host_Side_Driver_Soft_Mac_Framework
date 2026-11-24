
#include <stdio.h>
#include <pcap/pcap.h>

#define INTERFACE "wlan1"

int main()
{
    pcap_t *handle;
    char errbuf[PCAP_ERRBUF_SIZE];

    struct pcap_pkthdr *header;
    const u_char *packet;
    int result;

    handle = pcap_open_live(INTERFACE, 65535, 1, 1000, errbuf);

    if (!handle) {
        printf("Error: %s\n", errbuf);
        return 1;
    }

    printf("RX capture started on %s\n", INTERFACE);

    while (1) {
        result = pcap_next_ex(handle, &header, &packet);

/*        if (result == 1) {
            printf("RX packet received: %u bytes\n",
                   header->len);
        }
  */  }

    pcap_close(handle);
    return 0;
}
