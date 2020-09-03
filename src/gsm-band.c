#include <stdlib.h>
#include <string.h>

#include "gsm-band.h"

static struct gsm_band_t gsm_band[] = {
    // These are the most popular, and will take priority
    {    GSM_900,    "GSM 900",  935.0*MHZ,  959.8*MHZ,   0,  124, -45 },
    {   DCS_1800,   "DCS 1800", 1805.2*MHZ, 1879.8*MHZ, 512,  885, -95 },
    {    GSM_850,    "GSM 850",  869.2*MHZ,  893.8*MHZ, 128,  251, -45 },
    {  E_GSM_900,  "E-GSM 900",  925.2*MHZ,  934.8*MHZ, 975, 1023, -45 },

    // These have unique arfcns
    {    GSM_450,    "GSM 450",  460.6*MHZ,  467.4*MHZ, 259,  293, -10 },
    {    GSM_480,    "GSM 480",  489.6*MHZ,  495.8*MHZ, 306,  340, -10 },

    // For these, the band must be provided
    {   PCS_1900,   "PCS 1900", 1930.2*MHZ, 1989.8*MHZ, 512,  810, -80 },
    {  P_GSM_900,  "P-GSM 900",  935.2*MHZ,  959.8*MHZ,   1,  124, -45 },
    {  E_GSM_900,  "E-GSM 900",  935.0*MHZ,  959.8*MHZ,   0,  124, -45 },
    {  R_GSM_900,  "R-GSM 900",  935.0*MHZ,  959.8*MHZ,   0,  124, -45 },
    {  R_GSM_900,  "R-GSM 900",  921.2*MHZ,  934.8*MHZ, 955, 1023, -45 },
    { ER_GSM_900, "ER-GSM 900",  921.2*MHZ,  934.8*MHZ, 955, 1023, -45 },
    { ER_GSM_900, "ER-GSM 900",  935.0*MHZ,  959.8*MHZ,   0,  124, -45 },
    {    GSM_EOL,       "NULL",        0.0,        0.0,   0,    0,   0 },
};

int
band_arfcn_to_dl(int band, uint32_t arfcn, double *freq)
{
    struct gsm_band_t *b = gsm_band;
    while (b->band != GSM_EOL) {
        struct gsm_band_t *cur = b++;

        if (arfcn < cur->a_lo) {
            continue;
        }

        if (arfcn > cur->a_hi) {
            continue;
        }

        if (band != cur->band) {
            continue;
        }

        *freq = cur->dl_lo + (GSM_ARFCN_STEP * (double)(arfcn - cur->a_lo));
        return 0;
    }

    return 1;
}

int
band_arfcn_to_ul(int band, uint32_t arfcn, double *freq)
{
    struct gsm_band_t *b = gsm_band;
    while (b->band != GSM_EOL) {
        struct gsm_band_t *cur = b++;

        if (arfcn < cur->a_lo) {
            continue;
        }

        if (arfcn > cur->a_hi) {
            continue;
        }

        if (band != cur->band) {
            continue;
        }

        *freq = cur->dl_lo + (GSM_ARFCN_STEP * (double)(arfcn - cur->a_lo)) + cur->offset;
        return 0;
    }

    return 1;
}

int
arfcn_to_dl(uint32_t arfcn, double *freq)
{
    struct gsm_band_t *b = gsm_band;
    while (b->band != GSM_EOL) {
        struct gsm_band_t *cur = b++;

        if (arfcn < cur->a_lo) {
            continue;
        }

        if (arfcn > cur->a_hi) {
            continue;
        }

        *freq = cur->dl_lo + (GSM_ARFCN_STEP * (double)(arfcn - cur->a_lo));
        return 0;
    }

    return 1;
}

int
arfcn_to_ul(uint32_t arfcn, double *freq)
{
    struct gsm_band_t *b = gsm_band;
    while (b->band != GSM_EOL) {
        struct gsm_band_t *cur = b++;

        if (arfcn < cur->a_lo) {
            continue;
        }

        if (arfcn > cur->a_hi) {
            continue;
        }

        *freq = cur->dl_lo + (GSM_ARFCN_STEP * (double)(arfcn - cur->a_lo)) - cur->offset;
        return 0;
    }

    return 1;
}
