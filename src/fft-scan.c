#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <complex.h>
#include <sys/stat.h>
#include <liquid/liquid.h>

#define LOGEX_MAIN
#include <logex.h>
#include <envex.h>
#include <memex.h>

#include <bingewatch/sdrs.h>
#include <bingewatch/simple-buffers.h>

#include "lte-band.h"

#define MAX_RATE (double)30720000

static struct fft_res_s {
    uint32_t len;
    uint32_t use;
} fft_res[] = {
    {128, 100},
    {256, 200},
    {512, 500},
    {1024, 1000},
    {2048, 2000},
    {4096, 4000},
    {8192, 8000},
    {16384, 16000},
    {0,0}
};

struct scan_plan_t {
    POOL *pool; 
    uint32_t fftlen;
    uint32_t fftuse;

    double band_start;
    double band_end;
    double bin_res;
    double samp_rate;

    float *full_scan;
    size_t n_bins;
};

struct scan_rx_t {
    IO_HANDLE sdr;
    double freq;
    double samp_rate;
    double sec;
};

struct proc_rx_t {
    IO_HANDLE buf;
    uint32_t fftlen;
    uint32_t dec;
    float *fft;
};

static void
fft_mirror(float complex *out, float complex *in, uint32_t fftlen)
{
    uint32_t hlen = 1 + (fftlen - 1) / 2;
    memcpy(out, in + hlen, (fftlen - hlen) * sizeof(float complex));
    memcpy(out + (fftlen - hlen), in + 1, (hlen - 1) * sizeof(float complex));
}

static void
print_plan(struct scan_plan_t *plan)
{
    printf("Scan Plan (%f-%f):\n", plan->band_start, plan->band_end);
    printf("\t samp rate: %f\n", plan->samp_rate);
    printf("\tresolution: %f\n", plan->bin_res);
    printf("\t    fftlen: %d\n", plan->fftlen);
    printf("\t    fftuse: %d\n", plan->fftuse);
    printf("\n");
}

static void
print_rounds(struct scan_plan_t *plan)
{
    printf("Scan Rounds:\n");

    double half_band = plan->bin_res * (plan->fftuse / 2);
    double center_off = (plan->bin_res / 2);
    double f = plan->band_start + half_band - center_off;
    int round = 0;
    while ((f - half_band) < plan->band_end) {
        printf("  %2d: %f %f %f\n", round, f + center_off - half_band, f, f - center_off + half_band);
        f += 2 * half_band;
        round++;
    }
    printf("\n");
}

static int
plan_scan(double f0, double f1, double resolution, struct scan_plan_t *plan)
{
    memset(plan, 0, sizeof(struct scan_plan_t));
    plan->pool = create_pool();
    plan->band_start = f0;
    plan->band_end = f1;
    plan->bin_res = resolution;

    int valid = 0;
    int i = 0;
    while (fft_res[i].len) {
        struct fft_res_s *fr = fft_res + i;
        double rate = (double)fr->len * resolution;
        if (rate <= MAX_RATE) {
            goto next;
        } else if (!valid) {
            goto last;
        } else {
            break;
        }

    next:
        valid = 1;
        i++;

    last:
        plan->fftlen = fr->len;
        plan->fftuse = fr->use;
        plan->samp_rate = (double)fr->len * resolution;

        if (!valid) {
            return 1;
        }
    }

    return 0;
}

static void *
scan_rx(void *vars)
{
    struct scan_rx_t *scan = (struct scan_rx_t *)vars;

    size_t n_samp = (size_t)(scan->sec * scan->samp_rate);
    size_t total_bytes = n_samp * sizeof(float complex);

    IO_HANDLE out;
    size_t bufsize = total_bytes / 8;
    const IOM *rbm = new_rb_machine(&out, total_bytes, bufsize);

    soapy_rx_set_freq(scan->sdr, scan->freq);

    size_t remaining = total_bytes;
    float complex *buf = (float complex *)malloc(bufsize);
    while (remaining) {
        size_t bytes = (remaining > bufsize) ? bufsize : remaining;
        soapy_rx_machine->read(scan->sdr, buf, &bytes);
        remaining -= bytes;
        rbm->write(out, buf, &bytes);
    }
    printf("Freq %f: %zd bytes\n", scan->freq, rb_get_bytes(out));
    free(buf);

    IO_HANDLE *ret = malloc(sizeof(IO_HANDLE));
    *ret = out;
    pthread_exit(ret);
}

static void *
scan_process(void *vars)
{
    struct proc_rx_t *proc = (struct proc_rx_t *)vars;
    const IOM *rb = get_rb_machine();

    size_t fft_bytes = proc->fftlen * sizeof(float complex);
    float complex *fft_in = malloc(fft_bytes);
    float complex *fft_out = malloc(fft_bytes);

    size_t fft_skip_bytes = (proc->dec - 1) * proc->fftlen;
    uint8_t *fft_skip = malloc(fft_skip_bytes);

    fftplan fft = fft_create_plan(proc->fftlen, fft_in, fft_out, LIQUID_FFT_FORWARD, 0);

    size_t n_samp = (size_t)(rb_get_bytes(proc->buf) / (proc->fftlen * proc->dec));
    size_t total_samples = 0;
    printf("%zd\n", n_samp);
    while (rb_get_bytes(proc->buf) > 0) {
        size_t bytes = fft_bytes;
        rb->read(proc->buf, fft_in, &bytes);
        if (bytes < fft_bytes) {
            continue;
        }

        fft_execute(fft);
        float complex *f = (float complex *)fft_skip;
        fft_mirror(f, fft_out, proc->fftlen);

        for (int i = 0; i < proc->fftlen; i++) {
            // MAX
            //float f0 = cabsf(proc->fft[i]);
            //float f1 = cabsf(f[i]);
            //proc->fft[i] = (f0 > f1) ? f0 : f1;

            // AVG
            proc->fft[i] += cabsf(f[i]) / (float)n_samp;
        }
        total_samples++;

        bytes = fft_skip_bytes;
        rb->read(proc->buf, fft_skip, &bytes);
    }

    rb->destroy(proc->buf);
    fft_destroy_plan(fft);
    free(fft_in);
    free(fft_out);
    free(fft_skip);

    void *ret = NULL;
    pthread_exit(ret);
}

static void
execute_plan(struct scan_plan_t *plan)
{
    // Allocate memory for RX struct
    struct scan_rx_t *scan = (struct scan_rx_t *)palloc(plan->pool, sizeof(struct scan_rx_t));

    // Calculate number of RX rounds
    float rounds_f = (plan->band_end - plan->band_start) / (plan->bin_res * plan->fftuse);
    float rounds_dec = rounds_f - (float)(int)rounds_f;
    int n_rounds = (rounds_dec == 0) ? (int)rounds_f : (int)rounds_f + 1;

    // Allocate memory for workers
    pthread_t *workers = (pthread_t *)palloc(plan->pool, n_rounds * sizeof(pthread_t));
    struct proc_rx_t *process = (struct proc_rx_t *)palloc(plan->pool, n_rounds * sizeof(struct proc_rx_t));

    // Create SDR IOM
    IO_HANDLE lime = new_soapy_rx_machine("lime");
    if (!lime) {
        error("LimeSDR not found");
        return;
    }

    float lna, tia, pga;
    ENVEX_FLOAT(lna, "LIME_RX_GAIN_LNA", 0.0);
    ENVEX_FLOAT(tia, "LIME_RX_GAIN_TIA", 0.0);
    ENVEX_FLOAT(pga, "LIME_RX_GAIN_PGA", 0.0);
    soapy_set_gains(lime, lna, tia, pga);
    soapy_rx_set_samp_rate(lime, plan->samp_rate);

    scan->sdr = lime;
    scan->samp_rate = plan->samp_rate;

    double total_seconds;
    ENVEX_DOUBLE(total_seconds, "FFT_SCAN_SAMPLE_TIME", 1);
    scan->sec = total_seconds;

    // Init loop
    double half_band = plan->bin_res * (plan->fftuse / 2);
    double center_off = (plan->bin_res / 2);
    double f = plan->band_start + half_band - center_off;
    int r = 0;
    for (; r < n_rounds; r++) {
        scan->freq = f;

        pthread_t rx;
        pthread_create(&rx, NULL, scan_rx, (void *)scan);

        IO_HANDLE *h;
        pthread_join(rx, (void **)&h);

        ENVEX_UINT32(process[r].dec, "FFT_SCAN_SAMPLE_DECIMATE", 10);
        process[r].fftlen = plan->fftlen;
        process[r].buf = *h;
        process[r].fft = pcalloc(plan->pool, plan->fftlen * sizeof(float));
        pthread_create(&workers[r], NULL, scan_process, (void *)&process[r]);

        f += 2 * half_band;
    }

    for (int r = 0; r < n_rounds; r++) {
        pthread_join(workers[r], NULL);
    }

    // Allocate enough memory for all fft bins
    size_t bin_bytes = n_rounds * plan->fftlen * sizeof(float);
    float *bins = (float *)malloc(bin_bytes);

    size_t total_bins = 0;
    for (int r = 0; r < n_rounds; r++) {
        size_t offset = (plan->fftlen - plan->fftuse) / 2;
        float *fft = process[r].fft + offset;

        size_t n_bins = (r < (n_rounds - 1)) ? plan->fftuse : plan->fftuse + offset;
        size_t bytes = n_bins * sizeof(float);

        memcpy(bins + total_bins, fft, bytes);
        total_bins += n_bins;
    }

    plan->full_scan = bins;
    plan->n_bins = total_bins;
}

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
