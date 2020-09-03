#ifndef __LTE_BAND_H__
#define __LTE_BAND_H__

#include <stdint.h>
#include <cell-tools.h>

#define LTE_EARFCN_STEP 100*KHZ

enum lte_3gpp_release_e {
    LTE_3GPP_REL_8,
    LTE_3GPP_REL_8_3,
    LTE_3GPP_REL_8_4,
    LTE_3GPP_REL_9,
    LTE_3GPP_REL_10,
    LTE_3GPP_REL_10_1,
    LTE_3GPP_REL_10_3,
    LTE_3GPP_REL_10_4,
    LTE_3GPP_REL_11,
    LTE_3GPP_REL_11_1,
    LTE_3GPP_REL_11_3,
    LTE_3GPP_REL_12,
    LTE_3GPP_REL_12_4,
    LTE_3GPP_REL_13_2,
    LTE_3GPP_REL_13_3,
    LTE_3GPP_REL_15,
    LTE_3GPP_REL_UNKNOWN,
    LTE_3GPP_EOL,
};

struct lte_band_t {
    int band;
    double dl_lo;
    double dl_hi;
    int e_lo;
    int e_hi;
    double offset;
    char *name;
    int release;
};

void print_lte_band_csv();
int get_lte_band(uint32_t band_number, struct lte_band_t *band);
int band_earfcn_to_dl(uint32_t band, uint32_t earfcn, double *freq);
int band_earfcn_to_ul(uint32_t band, uint32_t earfcn, double *freq);
int earfcn_to_dl(uint32_t earfcn, double *freq);
int earfcn_to_ul(uint32_t earfcn, double *freq);
int earfcn_to_band(uint32_t earfcn);

#endif
