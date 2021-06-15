#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lte-band.h"

static struct lte_band_t lte_band[] = {
    { 1, 2110.0*MHZ, 2170.0*MHZ,    0,    599, 190*MHZ,     "2100", LTE_3GPP_REL_8},
    { 2, 1930.0*MHZ, 1990.0*MHZ,  600,   1199,  80*MHZ, "1900 PCS", LTE_3GPP_REL_8},
    { 3, 1805.0*MHZ, 1880.0*MHZ, 1200,   1949,  95*MHZ,    "1800+", LTE_3GPP_REL_8},
    { 4, 2110.0*MHZ, 2155.0*MHZ, 1950,   2399, 400*MHZ,    "AWS-1", LTE_3GPP_REL_8},
    { 5,  869.0*MHZ,  894.0*MHZ, 2400,   2649,  45*MHZ,      "850", LTE_3GPP_REL_8},
    { 7, 2620.0*MHZ, 2690.0*MHZ, 2750,   3449, 120*MHZ,     "2600", LTE_3GPP_REL_8},
    { 8,  925.0*MHZ,  960.0*MHZ, 3450,   3799,  35*MHZ,  "900 GSM", LTE_3GPP_REL_8},
    {12,  729.0*MHZ,  746.0*MHZ, 5010,   5179,  30*MHZ,    "700 a", LTE_3GPP_REL_8_4},
    {13,  746.0*MHZ,  756.0*MHZ, 5180,   5279, -31*MHZ,    "700 c", LTE_3GPP_REL_8},
    {14,  758.0*MHZ,  768.0*MHZ, 5280,   5379, -30*MHZ,   "700 PS", LTE_3GPP_REL_8},
    {17,  734.0*MHZ,  746.0*MHZ, 5730,   5849,  30*MHZ,    "700 b", LTE_3GPP_REL_8_3},
    {20,  791.0*MHZ,  821.0*MHZ, 6150,   6449, -41*MHZ,      "800", LTE_3GPP_REL_UNKNOWN},
    {25, 1930.0*MHZ, 1995.0*MHZ, 8040,   8689,  80*MHZ,    "1900+", LTE_3GPP_REL_10},
    {26,    859*MHZ,  894.0*MHZ, 8690,   9039,  45*MHZ,     "850+", LTE_3GPP_REL_11},
    {28,  758.0*MHZ,  803.0*MHZ, 9210,   9659,  55*MHZ,  "700 APT", LTE_3GPP_REL_11_1}, 
    {29,  717.0*MHZ,  728.0*MHZ, 9660,   9769,   0*MHZ,    "700 d", LTE_3GPP_REL_11_3},
    {30, 2350.0*MHZ, 2360.0*MHZ, 9770,   9869,  45*MHZ, "2300 WCS", LTE_3GPP_REL_12},
    {66, 2110.0*MHZ, 2200.0*MHZ, 66436, 67335, 400*MHZ,    "AWS-3", LTE_3GPP_REL_13_2},
    {71,  617.0*MHZ,  652.0*MHZ, 68586, 68935, -46*MHZ,      "600", LTE_3GPP_REL_15},
    { 0,        0.0,        0.0,     0,     0,     0.0 ,    "NULL", LTE_3GPP_EOL},
};

static uint32_t
get_n_bands()
{
    struct lte_band_t *b = lte_band;
    uint32_t n = 0;

    while (b->band != 0) {
        n++;
        b++;
    }
    return n;
}

static void
print_csv_titles()
{
    printf("band,name,earfcn_lo,earfcn_hi,dl_lo,dl_hi,ul_lo,ul_hi\n");
}

static void
print_csv_info(struct lte_band_t *band)
{
    printf("%d,%s,%d,%d,%.0f,%.0f,%.0f,%.0f\n",
        band->band, band->name, band->e_lo, band->e_hi, band->dl_lo, band->dl_hi,
        band->dl_lo - band->offset, band->dl_hi - band->offset);
}

void
print_lte_band_csv()
{
    print_csv_titles();

    struct lte_band_t *b = lte_band;
    while (b->band != 0) {
        print_csv_info(b);
        b++;
    }
}

int
get_lte_band(uint32_t band_number, struct lte_band_t *band)
{
    struct lte_band_t *b = lte_band;

    while (b->band != 0) {
        if (b->band == band_number) {
            memcpy(band, b, sizeof(struct lte_band_t));
            return 0;
        }
        b++;
    }

    return 1;
}

uint32_t
dl_to_band(double freq, struct lte_band_t **band)
{
    struct lte_band_t *ret = malloc(get_n_bands() * sizeof(struct lte_band_t));
    uint32_t n = 0;

    struct lte_band_t *b = lte_band;
    while (b->band != 0) {
        if (freq < b->dl_lo) {
            goto next;
        }

        if (freq >= b->dl_hi) {
            goto next;
        }

        memcpy(ret + n, b, sizeof(struct lte_band_t));
        n++;

    next:
        b++;
    }

    if (n == 0) {
        free(ret);
        ret = NULL;
    }

    *band = ret;
    return n;
}

uint32_t
ul_to_band(double freq, struct lte_band_t **band)
{
    struct lte_band_t *ret = malloc(get_n_bands() * sizeof(struct lte_band_t));
    uint32_t n = 0;

    struct lte_band_t *b = lte_band;
    while (b->band != 0) {
        // Handle no-uplink case
        if (b->offset == 0) {
            goto next;
        }

        double ul_lo = b->dl_lo - b->offset;
        double ul_hi = b->dl_hi - b->offset;
        if (freq < ul_lo) {
            goto next;
        }

        if (freq >= ul_hi) {
            goto next;
        }

        memcpy(ret + n, b, sizeof(struct lte_band_t));
        n++;

    next:
        b++;
    }

    if (n == 0) {
        free(ret);
        ret = NULL;
    }

    *band = ret;
    return n;
}

int
band_earfcn_to_dl(uint32_t band, uint32_t earfcn, double *freq)
{
    struct lte_band_t b;
    if (get_lte_band(band, &b) != 0) {
        return 1;
    }

    if (earfcn < b.e_lo) {
        return 1;
    }

    if (earfcn > b.e_hi) {
        return 1;
    }

    *freq = b.dl_lo + (LTE_EARFCN_STEP * (double)(earfcn - b.e_lo));
    return 0;
}

int
band_earfcn_to_ul(uint32_t band, uint32_t earfcn, double *freq)
{
    struct lte_band_t b;
    if (get_lte_band(band, &b) != 0) {
        return 1;
    }

    if (earfcn < b.e_lo) {
        return 1;
    }

    if (earfcn > b.e_hi) {
        return 1;
    }

    *freq = b.dl_lo + (LTE_EARFCN_STEP * (double)(earfcn - b.e_lo)) - b.offset;
    return 0;
}

int
earfcn_to_dl(uint32_t earfcn, double *freq)
{
    struct lte_band_t *b = lte_band;
    while (b->band != 0) {
        struct lte_band_t *cur = b++;

        if (earfcn < cur->e_lo) {
            continue;
        }

        if (earfcn > cur->e_hi) {
            continue;
        }

        *freq = cur->dl_lo + (LTE_EARFCN_STEP * (double)(earfcn - cur->e_lo));
        return 0;
    }

    return 1;
}

int
earfcn_to_ul(uint32_t earfcn, double *freq)
{
    struct lte_band_t *b = lte_band;
    while (b->band != 0) {
        struct lte_band_t *cur = b++;

        if (earfcn < cur->e_lo) {
            continue;
        }

        if (earfcn > cur->e_hi) {
            continue;
        }

        *freq = cur->dl_lo + (LTE_EARFCN_STEP * (double)(earfcn - cur->e_lo)) - cur->offset;
        return 0;
    }

    return 1;
}

int
earfcn_to_band(uint32_t earfcn)
{
    struct lte_band_t *b = lte_band;
    while (b->band != 0) {
        struct lte_band_t *cur = b++;

        if (earfcn < cur->e_lo) {
            continue;
        }

        if (earfcn > cur->e_hi) {
            continue;
        }

        return cur->band;
    }

    return -1;
}
