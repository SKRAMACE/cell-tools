#ifndef __GSM_BAND_H__
#define __GSM_BAND_H__

#include <stdint.h>
#include <cell-tools.h>

#define GSM_ARFCN_STEP 200*KHZ

#define GSM_BAND_STR(x) \
    (x == GSM_900) ? "GSM-900" : \
    (x == DCS_1800) ? "DCS-1800" : \
    (x == PCS_1900) ? "PCS-1900" : \
    (x == GSM_450) ? "GSM-450" : \
    (x == GSM_480) ? "GSM-480" : \
    (x == GSM_850) ? "GSM-850" : \
    (x == P_GSM_900) ? "P-GSM" : \
    (x == E_GSM_900) ? "E-GSM" : \
    (x == R_GSM_900) ? "R-GSM" : \
    (x == ER_GSM_900) ? "ER-GSM" : \
    "UNKNOWN"

enum gsm_band_e {
    GSM_900,
    DCS_1800,
    PCS_1900,
    GSM_450,
    GSM_480,
    GSM_850,
    P_GSM_900,
    E_GSM_900,
    R_GSM_900,
    ER_GSM_900,
    GSM_EOL,
};

struct gsm_band_t {
    enum gsm_band_e band;
    const char *name;
    double dl_lo;
    double dl_hi;
    int a_lo;
    int a_hi;
    double offset;
};

int band_arfcn_to_dl(int band, uint32_t arfcn, double *freq);
int arfcn_to_dl(uint32_t arfcn, double *freq);
int arfcn_to_ul(uint32_t arfcn, double *freq);
int arfcn_to_band(uint32_t arfcn);

#endif
