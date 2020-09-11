#ifndef __FFT_SCAN_H__
#define __FFT_SCAN_H__

#include <memex.h>

struct scan_plan_t {
    POOL *pool; 
    uint32_t fftlen;
    uint32_t fftuse;

    uint32_t bin_bw;
    uint32_t bin_dc;

    double band_start;
    double band_end;
    double bin_res;
    double samp_rate;

    float *full_scan;
    size_t n_bins;
};

void print_plan(struct scan_plan_t *plan);
void print_rounds(struct scan_plan_t *plan);

int plan_scan(double f0, double f1, double resolution, struct scan_plan_t *plan);
void execute_plan(struct scan_plan_t *plan);

#endif
