#include <stdlib.h>
#include <complex.h>
#include <liquid/liquid.h>
#include <semaphore.h>

#define LOGEX_TAG "FFTSCAN"
#define LOGEX_STD
#include <logex.h>
#include <envex.h>

#include <bingewatch/sdrs.h>
#include <bingewatch/simple-buffers.h>

#include "fft-scan.h"

#define ASSERT_ENV(x,r)\
    if (!ENVEX_EXISTS(x)) {\
        error("Missing environment variable \"%s\"", x);\
        return r;\
    }

//#define MAX_RATE (double)30720000
#define MAX_RATE (double)20000000

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

struct scan_rx_t {
    IO_HANDLE sdr;
    double freq;
    double samp_rate;
    double sec;
    uint32_t dec;
    uint32_t fftlen;
    sem_t *thread_limit;

    int (*set_freq)(int, double);
    int (*read)(int, void*, size_t*);
};

struct proc_rx_t {
    IO_HANDLE buf;
    uint32_t fftlen;
    float *fft;
    double freq;
    sem_t *thread_limit;
};

static uint32_t
calculate_step_bins(struct scan_plan_t *plan)
{
    uint32_t fft_half = plan->fftlen / 2;
    return (plan->bin_dc == 0) ? 2 * ((plan->fftlen / 2) - plan->bin_bw) : fft_half - plan->bin_dc - plan->bin_bw;
}

static void
fft_mirror(float complex *out, float complex *in, uint32_t fftlen)
{
    uint32_t hlen = 1 + (fftlen - 1) / 2;
    memcpy(out, in + hlen, (fftlen - hlen) * sizeof(float complex));
    memcpy(out + (fftlen - hlen), in + 1, (hlen - 1) * sizeof(float complex));
}

void
print_plan(struct scan_plan_t *plan)
{
    printf("Scan Plan (%f-%f):\n", plan->band_start, plan->band_end);
    printf("\t samp rate: %f\n", plan->samp_rate);
    printf("\tresolution: %f\n", plan->bin_res);
    printf("\t    fftlen: %d\n", plan->fftlen);
    printf("\t    fftuse: %d\n", plan->fftuse);
    printf("\t    bin_bw: %d\n", plan->bin_bw);
    printf("\t    bin_dc: %d\n", plan->bin_dc);
    printf("\n");
}

void
print_rounds(struct scan_plan_t *plan)
{
    printf("Scan Rounds:\n");

    double fft_half = (double)(plan->fftlen / 2);
    double half_band = plan->bin_res * fft_half;
    double bw_band = (double)plan->bin_bw * plan->bin_res;
    double dc_band = (double)plan->bin_dc * plan->bin_res;
    double half_bin = (plan->bin_res / 2);
    double f = plan->band_start + half_band - bw_band - half_bin;

    uint32_t step_bins = calculate_step_bins(plan);
    double step = (double)step_bins * plan->bin_res;

    /*
    double center_off = (plan->bin_res / 2);
    double f = plan->band_start + half_band - center_off;
    */
    int round = 0;
    while ((f - half_band) < plan->band_end) {
        printf("  %2d: %f %f %f\n",
            round, f + half_bin - half_band, f, f - half_bin + half_band);
        f += step;
        round++;
    }
    printf("\n");
}

int
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
        ENVEX_UINT32(plan->bin_bw, "FFT_SCAN_BIN_BW", (plan->fftlen - plan->fftuse) / 2);
        ENVEX_UINT32(plan->bin_dc, "FFT_SCAN_BIN_DC", 1);

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

    sem_wait(scan->thread_limit);
    size_t total_samples = (size_t)(scan->sec * scan->samp_rate);

    size_t fft_bytes = scan->fftlen * sizeof(float complex);
    size_t read_bytes = scan->dec * scan->fftlen * sizeof(float complex);
    uint8_t *read_buf = malloc(read_bytes);

    IO_HANDLE out  = new_rb_machine();

    scan->set_freq(scan->sdr, scan->freq);

    size_t remaining = total_samples * sizeof(float complex);
    while (remaining) {
        size_t bytes = (read_bytes < remaining) ? read_bytes : remaining;
        scan->read(scan->sdr, read_buf, &bytes);
        remaining -= bytes;

        bytes = (bytes > fft_bytes) ? fft_bytes : bytes;
        rb_machine->write(out, read_buf, &bytes);
    }
    printf("Freq %f: %zd bytes\n", scan->freq, rb_get_size(out));
    free(read_buf);

    sem_post(scan->thread_limit);

    IO_HANDLE *ret = malloc(sizeof(IO_HANDLE));
    *ret = out;
    pthread_exit(ret);
}

static void *
scan_process(void *vars)
{
    struct proc_rx_t *proc = (struct proc_rx_t *)vars;
    sem_wait(proc->thread_limit);

    const IOM *rb = get_rb_machine();

    size_t fft_bytes = proc->fftlen * sizeof(float complex);
    float complex *fft_in = malloc(fft_bytes);
    float complex *fft_out = malloc(fft_bytes);
    uint8_t *fft_temp = malloc(fft_bytes);

    fftplan fft = fft_create_plan(proc->fftlen, fft_in, fft_out, LIQUID_FFT_FORWARD, 0);

    size_t n_samp = (size_t)(rb_get_size(proc->buf) / proc->fftlen);
    size_t total_samples = 0;
    printf("%zd\n", n_samp);
    while (rb_get_size(proc->buf) > 0) {
        size_t bytes = fft_bytes;
        rb->read(proc->buf, fft_in, &bytes);
        if (bytes < fft_bytes) {
            continue;
        }

        fft_execute(fft);
        float complex *f = (float complex *)fft_temp;
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
    }

    rb->destroy(proc->buf);
    fft_destroy_plan(fft);
    free(fft_in);
    free(fft_out);
    free(fft_temp);

    sem_post(proc->thread_limit);
    void *ret = NULL;
    pthread_exit(ret);
}

static void
b210_setup(struct scan_rx_t *scan)
{
    IO_HANDLE b210 =
        new_b210_rx_machine_devstr("recv_frame_size=2056,num_recv_frames=2038");

    float lna;
    ENVEX_FLOAT(lna, "FFT_SCAN_GAIN_LNA", 0.0);
    b210_rx_set_gain(b210, lna);

    b210_rx_set_samp_rate(b210, scan->samp_rate);
    b210_rx_set_bandwidth(b210, scan->samp_rate);

    scan->set_freq = b210_rx_set_freq;
    scan->read = b210_rx_machine->read;

    scan->sdr = b210;
}

static void
lime_setup(struct scan_rx_t *scan)
{
    IO_HANDLE lime = new_lime_rx_machine();

    float lna, tia, pga;
    ENVEX_FLOAT(lna, "FFT_SCAN_GAIN_LNA", 0.0);
    ENVEX_FLOAT(tia, "FFT_SCAN_GAIN_TIA", 0.0);
    ENVEX_FLOAT(pga, "FFT_SCAN_GAIN_PGA", 0.0);
    lime_set_gains(lime, lna, tia, pga);

    float freq, samp_rate, bw;
    lime_rx_set_samp_rate(lime, scan->samp_rate);
    lime_rx_set_bandwidth(lime, scan->samp_rate);

    scan->set_freq = lime_rx_set_freq;
    scan->read = lime_rx_machine->read;

    scan->sdr = lime;
}


void
execute_plan(struct scan_plan_t *plan)
{
    // Allocate memory for RX struct
    struct scan_rx_t *scan = (struct scan_rx_t *)palloc(plan->pool, sizeof(struct scan_rx_t));

    scan->samp_rate = plan->samp_rate;
    scan->fftlen = plan->fftlen;

    // Create SDR IOM
    IO_HANDLE input = 0;
    if (ENVEX_EXISTS("FFT_SCAN_B210_INPUT")) {
        info("Input: file");
        b210_setup(scan);

    } else if (ENVEX_EXISTS("FFT_SCAN_LIME_INPUT")) {
        info("Input: lime");
        lime_setup(scan);

    } else {
        info("Default: lime");
        lime_setup(scan);
    }

    if (scan->sdr == 0) {
        error("Input setup failed");
        return;
    }

    double total_seconds;
    ENVEX_DOUBLE(total_seconds, "FFT_SCAN_SAMPLE_TIME", 1);
    scan->sec = total_seconds;
    ENVEX_UINT32(scan->dec, "FFT_SCAN_SAMPLE_DECIMATE", 10);

    // Init loop
    double fft_half = (double)(plan->fftlen / 2);
    double half_band = plan->bin_res * fft_half;
    double bw_band = (double)plan->bin_bw * plan->bin_res;
    double dc_band = (double)plan->bin_dc * plan->bin_res;
    double half_bin = (plan->bin_res / 2);
    double f = plan->band_start + half_band - bw_band - half_bin;

    uint32_t step_bins = calculate_step_bins(plan);
    double step = (double)step_bins * plan->bin_res;

    // Calculate number of RX rounds
    float rounds_f = (plan->band_end - plan->band_start) / step;
    float rounds_dec = rounds_f - (float)(int)rounds_f;
    int n_rounds = (rounds_dec == 0) ? (int)rounds_f : (int)rounds_f + 1;

    // Allocate memory for workers
    pthread_t *workers = (pthread_t *)palloc(plan->pool, n_rounds * sizeof(pthread_t));
    struct proc_rx_t *process = (struct proc_rx_t *)palloc(plan->pool, n_rounds * sizeof(struct proc_rx_t));

    sem_t thread_limit;
    sem_init(&thread_limit, 0, 4);
    scan->thread_limit = &thread_limit;

    int r = 0;
    for (; r < n_rounds; r++) {
        scan->freq = f;

        pthread_t rx;
        pthread_create(&rx, NULL, scan_rx, (void *)scan);

        IO_HANDLE *h;
        pthread_join(rx, (void **)&h);

        process[r].fftlen = plan->fftlen;
        process[r].buf = *h;
        process[r].freq = f;
        process[r].fft = pcalloc(plan->pool, plan->fftlen * sizeof(float));
        process[r].thread_limit = &thread_limit;
        pthread_create(&workers[r], NULL, scan_process, (void *)&process[r]);

        f += step;
    }

    for (int r = 0; r < n_rounds; r++) {
        pthread_join(workers[r], NULL);
    }

    // Allocate enough memory for all fft bins
    size_t bin_samples = ((plan->band_end - plan->band_start) / plan->bin_res) + 1;
    //size_t bin_samples = n_rounds * plan->fftlen;

    float *bins = (float *)calloc(bin_samples, sizeof(float));
    uint32_t *count = (uint32_t *)calloc(bin_samples, sizeof(uint32_t));

    size_t total_bins = 0;
    uint32_t b = 0;
    for (int r = 0; r < n_rounds; r++) {
        for (int off = plan->bin_bw; off < (plan->fftlen / 2) - plan->bin_dc; off++) {
            uint32_t bl = b + off - plan->bin_bw;
            uint32_t br = b + plan->fftlen - 1 - off - plan->bin_bw;

            if (bl < bin_samples) {
                bins[bl] += process[r].fft[off];
                count[bl]++;
            }

            if (br < bin_samples) {
                bins[br] += process[r].fft[plan->fftlen - 1 - off];
                count[br]++;
            }
        }
        b += step_bins;
    }

    b = 0;
    f = plan->band_start;
    for (; b < bin_samples; b++) {
        if (count[b] == 0) {
            break;
        }
        f += plan->bin_res;
        bins[b] = (count[b]) ? bins[b] / (float)count[b] : 0;
    }
    printf("%f\n", f);

    /*
    size_t total_bins = 0;
    for (int r = 0; r < n_rounds; r++) {
        size_t offset = (plan->fftlen - plan->fftuse) / 2;
        float *fft = process[r].fft + offset;

        size_t n_bins = (r < (n_rounds - 1)) ? plan->fftuse : plan->fftuse + offset;
        size_t bytes = n_bins * sizeof(float);

        memcpy(bins + total_bins, fft, bytes);
        total_bins += n_bins;
    }
    */

    free(count);

    plan->full_scan = bins;
    plan->n_bins = bin_samples;
}
