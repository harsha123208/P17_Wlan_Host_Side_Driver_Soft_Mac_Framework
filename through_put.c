
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>

#define DURATION 1

uint64_t read_bytes(const char *interface, const char *type)
{
    char path[256];
    FILE *fp;
    uint64_t bytes = 0;

    sprintf(path,"/sys/class/net/%s/statistics/%s",interface, type);

    fp = fopen(path, "r");

    if (fp == NULL) 
    {
        perror("fopen");
        return 0;
    }

    if (fscanf(fp, "%lu", &bytes) != 1) 
     {
          printf("Unable to read %s\n", path);
         fclose(fp);
          return 0;
     }

    fclose(fp);
    return bytes;
}

int main(int argc, char *argv[])
{
    const char *interface = (argc > 1) ? argv[1] : "wlan1";

    uint64_t tx_start=0, tx_end=0;
    uint64_t rx_start=0, rx_end=0;

    struct timespec start={0}, end={0};

    double elapsed=0;
    double tx_mbps=0, rx_mbps=0;

    printf("Interface: %s\n", interface);
    printf("Measuring for %d seconds...\n", DURATION);

    /* Initial interface counters */
    tx_start = read_bytes(interface, "tx_bytes");
    rx_start = read_bytes(interface, "rx_bytes");

    clock_gettime(CLOCK_MONOTONIC, &start);

    sleep(DURATION);

    clock_gettime(CLOCK_MONOTONIC, &end);

    /* Final interface counters */
    tx_end = read_bytes(interface, "tx_bytes");
    rx_end = read_bytes(interface, "rx_bytes");

    elapsed = (end.tv_sec - start.tv_sec) +(end.tv_nsec - start.tv_nsec) / 1e9;

    if (tx_end < tx_start || rx_end < rx_start) {
        fprintf(stderr, "Interface counters were reset\n");
        return 1;
    }

    tx_mbps = ((tx_end - tx_start) * 8.0) /(elapsed * 1000000.0);

    rx_mbps = ((rx_end - rx_start) * 8.0) /(elapsed * 1000000.0);

    printf("\n========== INTERFACE STATISTICS ==========\n");

    printf("Interface       : %s\n", interface);
    printf("Elapsed time    : %.3f seconds\n", elapsed);

    printf("TX bytes        : %%%llu\n,tx_end - tx_start);

    printf("RX bytes        : %%llu\n"\n",rx_end - rx_start);

    printf("TX throughput   : %.3f Mbps\n", tx_mbps);
    printf("RX throughput   : %.3f Mbps\n", rx_mbps);

    printf("==========================================\n");

    return 0;
}
