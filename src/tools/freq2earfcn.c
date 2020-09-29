#include <stdlib.h>
#include <stdio.h>

#include "lte-band.h"

static void
handle_downlink_earfcns(double freq)
{
    struct lte_band_t *dl = NULL;
    uint32_t n_bands = dl_to_band(freq, &dl);
    if (!dl) {
        return;
    }

    printf("Downlink:\n");
    uint32_t i = 0;
    for (; i < n_bands; i++) {
        uint32_t earfcn = dl[i].e_lo + (uint32_t)((freq - dl[i].dl_lo) / (double)LTE_EARFCN_STEP);
        printf("\tBand %2d: %d\n", dl[i].band, earfcn);
    }
    printf("\n");
    free(dl);
}

static void
handle_uplink_earfcns(double freq)
{
    struct lte_band_t *ul = NULL;
    uint32_t n_bands = ul_to_band(freq, &ul);
    if (!ul) {
        return;
    }

    printf("Uplink:\n");
    uint32_t i = 0;
    for (; i < n_bands; i++) {
        double dl_freq = freq + ul[i].offset;
        uint32_t earfcn = ul[i].e_lo + (uint32_t)((dl_freq - ul[i].dl_lo) / (double)LTE_EARFCN_STEP);

        printf("\tBand %2d: %d\n", ul[i].band, earfcn);
    }
    printf("\n");
    free(ul);
}

int
main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("usage: %s FREQ_IN_MHZ\n", argv[0]);
        printf("\n");
        exit(1);
    }

    double freq = atof(argv[1]) * MHZ;

    handle_downlink_earfcns(freq);
    handle_uplink_earfcns(freq);
}
