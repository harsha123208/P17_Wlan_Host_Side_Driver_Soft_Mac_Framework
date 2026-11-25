
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pcap/pcap.h>
#include <endian.h>

#define INTERFACE "mon0"
#define MAX_PACKET_SIZE 2000
#define PACKET_DATA 20

struct radiotap_header
{
    uint8_t version;
    uint8_t pad;
    uint16_t len;
    uint32_t present;
} __attribute__((packed));

struct ieee80211_hdr
{
    uint16_t frame_control;
    uint16_t duration_id;
    unsigned char addr1[6];
    unsigned char addr2[6];
    unsigned char addr3[6];
    uint16_t seq_ctrl;
} __attribute__((packed));

struct wifi_packet
{
    struct radiotap_header rtap;
    struct ieee80211_hdr mac;
    unsigned char payload[];
} __attribute__((packed));


void print_mac(const char *label, const unsigned char *addr)
{
    printf("%s = %02X:%02X:%02X:%02X:%02X:%02X\n",label, addr[0], addr[1], addr[2],addr[3], addr[4], addr[5]);
}


/* TRANSMISSION FUNCTION */

int transmit_packet(pcap_t *handle)
{
    char *input = malloc(PACKET_DATA);

    if (input == NULL)
    {
        printf("Input allocation failed\n");
        return EOF;
    }

    printf("\n--- TRANSMISSION ---\n");
    printf("Enter the data = ");

    if (scanf("%s", input) == EOF)
    {
        free(input);
        return EOF;
    }

    size_t payload_len = strlen(input) + 1;

    size_t packet_len = sizeof(struct wifi_packet) + payload_len;

    struct wifi_packet *pkt = calloc(1, packet_len);

    if (pkt == NULL)
    {
        printf("Packet allocation failed\n");
        free(input);
        return EOF;
    }

    /* Radiotap header */

    pkt->rtap.version = 0;
    pkt->rtap.pad = 0;
    pkt->rtap.len = htole16(8);
    pkt->rtap.present = htole32(0);

    /* MAC header */

    pkt->mac.frame_control = htole16(0x0008);
    pkt->mac.duration_id = htole16(0);

    unsigned char dest[6] ={0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    unsigned char src[6] ={0x02, 0x11, 0x22, 0x33, 0x44, 0x55};

    unsigned char bssid[6] ={0x02, 0x11, 0x22, 0x33, 0x44, 0x66};

    memcpy(pkt->mac.addr1, dest, 6);
    memcpy(pkt->mac.addr2, src, 6);
    memcpy(pkt->mac.addr3, bssid, 6);

    pkt->mac.seq_ctrl = htole16(0x0010);

    /* Payload */

    memcpy(pkt->payload, input, payload_len);

    printf("TX Payload = %s\n", input);
    printf("TX Payload length = %zu bytes\n", payload_len);
    printf("TX Total packet length = %zu bytes\n", packet_len);

    int result = pcap_inject(handle, pkt, packet_len);

    if (result < 0)
        printf("Injection failed: %s\n", pcap_geterr(handle));
    else
        printf("Injected %d bytes\n", result);

    free(pkt);
    free(input);
}


/* RECEPTION FUNCTION */

void receive_packet(pcap_t *handle)
{
    struct pcap_pkthdr *header;
    const unsigned char *data;

    int result;

    printf("\n--- RECEPTION ---\n");

    while (1)
    {
        result = pcap_next_ex(handle, &header, &data);

        if (result == 0)
            continue;

        if (result == -1)
        {
            printf("Capture error: %s\n", pcap_geterr(handle));
            break;
        }

        if (result == -2)
        {   printf("loop break:%s\n",pcap_geterr(handle));
            break;
        }

        printf("\n========== RX PACKET ==========\n");

        printf("Total packet length = %u bytes\n", header->len);
        printf("Captured length = %u bytes\n", header->caplen);

        /* Radiotap validation */

        if (header->caplen < 8)
        {
            printf("Invalid Radiotap header\n");
            continue;
        }

        unsigned int radiotap_len = data[2] | ((unsigned int)data[3] << 8);

        if (radiotap_len < 8 || radiotap_len > header->caplen)
        {
            printf("Invalid Radiotap length\n");
            continue;
        }

        printf("Radiotap length = %u bytes\n", radiotap_len);

        /* Extract 802.11 frame */

        const unsigned char *mac_frame = data + radiotap_len;

        unsigned int mac_len = header->caplen - radiotap_len;

        printf("802.11 frame length = %u bytes\n", mac_len);

        if (mac_len < 24)
        {
            printf("Invalid or incomplete MAC header\n");
            continue;
        }

        /* Frame Control */

        uint16_t fc =mac_frame[0] | ((uint16_t)mac_frame[1] << 8);

        printf("\nFrame Control = 0x%04X\n", fc);

        unsigned int frame_type = (fc >> 2) & 0x03;
        unsigned int frame_subtype = (fc >> 4) & 0x0F;

        printf("Frame type = %u\n", frame_type);
        printf("Frame subtype = %u\n", frame_subtype);

        if (frame_type != 2)
        {
            printf("Not a data frame\n");
            continue;
        }

        /* MAC addresses */

        print_mac("Destination Address", mac_frame + 4);
        print_mac("Source Address", mac_frame + 10);
        print_mac("BSSID", mac_frame + 16);

        /* Sequence Control */

        uint16_t seq_ctrl =mac_frame[22] | ((uint16_t)mac_frame[23] << 8);

        printf("Sequence Number = %u\n", seq_ctrl >> 4);
        printf("Fragment Number = %u\n", seq_ctrl & 0x0F);

        /* Payload */

        unsigned int payload_offset = 24;
        unsigned int payload_len = mac_len - payload_offset;

        printf("\nMAC header length = %u bytes\n", payload_offset);
        printf("Payload length = %u bytes\n", payload_len);

        printf("Payload = ");
        printf("%s", mac_frame + payload_offset);

        printf("\n========== END RX PACKET ==========\n");

        break;
    }
}


/* MAIN FUNCTION */

int main(void)
{
    char errbuf[PCAP_ERRBUF_SIZE];

    pcap_t *handle = NULL;

    handle = pcap_open_live(INTERFACE,MAX_PACKET_SIZE,1,-1,errbuf);

    if (handle == NULL)
    {
        printf("Open failed: %s\n", errbuf);
        return 1;
    }

    if (pcap_datalink(handle) != DLT_IEEE802_11_RADIO)
    {
        printf("Radiotap interface required\n");
        pcap_close(handle);
        return 1;
    }

    printf("Monitor interface opened successfully\n");
    while(1)
    {
    if(transmit_packet(handle)==EOF)
    {
     break;
    }

    receive_packet(handle);
    }
    pcap_close(handle);

    printf("\nTransmission and reception completed\n");

    return 0;
}
