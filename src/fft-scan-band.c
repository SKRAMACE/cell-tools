#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>

#define LOGEX_MAIN
#include <logex.h>
#include <envex.h>

#include "fft-scan.h"
#include "lte-band.h"

int
main(int argc, void *argv[])
{
    if (!ENVEX_EXISTS("FFT_SCAN_BAND")) {
        printf("ERROR: Missing environment variable \"FFT_SCAN_BAND\"\n");
        exit(1);
    }

    char *outdir;
    ENVEX_TOSTR(outdir, "FFT_SCAN_OUTDIR", "/tmp");
    struct stat s;
    if (stat(outdir, &s) != 0) {
        printf("ERROR: Output directory \"%s\" not found\n", outdir);
        exit(1);
    }

    if (!S_ISDIR(s.st_mode)) {
        printf("ERROR: \"%s\" is not a directory\n", outdir);
        exit(1);
    }

    uint32_t b;
    ENVEX_UINT32(b, "FFT_SCAN_BAND", 0);

    struct lte_band_t band;
    if (get_lte_band(b, &band) != 0) {
        printf("ERROR: Unsupported band (%d)\n", b);
        exit(1);
    }

    double bandwidth = band.dl_hi - band.dl_lo;
    double res;
    ENVEX_DOUBLE(res, "FFT_SCAN_RESOLUTION", bandwidth/10000);

    struct scan_plan_t plan;
    if (plan_scan(band.dl_lo, band.dl_hi, res, &plan) != 0) {
        printf("ERROR: Bin resolultion of %f results in impossible sample rate (%f)\n",
            plan.bin_res, plan.samp_rate);
        exit(1);
    }

    print_plan(&plan);
    print_rounds(&plan);

    execute_plan(&plan);

    char fname[1024];
    snprintf(fname, 1024, "%s/fftscan-band%d-binres%.0f.float", outdir, band.band, plan.bin_res);
    FILE *f = fopen(fname, "w");
    fwrite(plan.full_scan, sizeof(float), plan.n_bins, f);
    fclose(f);

    pool_cleanup();
}
