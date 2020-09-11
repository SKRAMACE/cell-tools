#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>

#define LOGEX_MAIN
#include <logex.h>
#include <envex.h>

#include "fft-scan.h"

int
main(int argc, void *argv[])
{
    if (!ENVEX_EXISTS("FFT_SCAN_START_FREQ")) {
        printf("ERROR: Missing environment variable \"FFT_SCAN_START_FREQ\"\n");
        exit(1);
    }

    double f0, f1, bw;
    ENVEX_DOUBLE(f0, "FFT_SCAN_START_FREQ", 0);

    if (ENVEX_EXISTS("FFT_SCAN_STOP_FREQ")) {
        ENVEX_DOUBLE(f1, "FFT_SCAN_START_FREQ", 0);
        bw = f1 - f0;

    } else if (ENVEX_EXISTS("FFT_SCAN_BANDWIDTH")) {
        ENVEX_DOUBLE(bw, "FFT_SCAN_BANDWIDTH", 0);
        f1 = f0 + bw;

    } else {
        printf("ERROR: Cannot determine bandwidth: Set either \"%s\" or \"%s\"\n",
            "FFT_SCAN_STOP_FREQ", "FFT_SCAN_BANDWIDTH");
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

    double res;
    ENVEX_DOUBLE(res, "FFT_SCAN_RESOLUTION", bw/10000);

    struct scan_plan_t plan;
    if (plan_scan(f0, f1, res, &plan) != 0) {
        printf("ERROR: Bin resolultion of %f results in impossible sample rate (%f)\n",
            plan.bin_res, plan.samp_rate);
        exit(1);
    }

    print_plan(&plan);
    print_rounds(&plan);

    execute_plan(&plan);

    char fname[1024];
    snprintf(fname, 1024, "%s/fftscan-freq%.0f-bw%.0f-binres%.0f.float",
        outdir, plan.band_start, plan.band_end - plan.band_start, plan.bin_res);
    FILE *f = fopen(fname, "w");
    fwrite(plan.full_scan, sizeof(float), plan.n_bins, f);
    fclose(f);

    pool_cleanup();
}
