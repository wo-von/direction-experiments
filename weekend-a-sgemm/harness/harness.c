/*
 * Timing and verification harness for weekend A.
 *
 * usage: build/<kernel> [-r reps] [-c] [n ...]     (default sizes: 512 1024 2048)
 *   -r reps  timed runs per size after one warm-up run (default 3)
 *   -c       CSV output
 * Set SGEMM_PEAK_GFLOPS to print % of peak.
 *
 * Verification is in double precision:
 * - n <= 256: every element of C is compared with an exact dot product.
 * - larger n: 64 random elements, plus 2 Freivalds checks (C*x against
 *   A*(B*x) for random x), which see an error anywhere in C. This is O(n^2),
 *   so it costs almost nothing next to the kernel and barely shows in perf.
 * The tolerance is statistical, not worst case: for an element with products
 * p_k it is 4 * n * FLT_EPSILON * rms(p), about 10x the error of a plain
 * sequential float sum on random data. Any loop order, blocking, SIMD or FMA
 * passes; a single dropped or doubled term fails. (The rigorous bound
 * n * eps * sum|p| is ~n times looser and lets such bugs through at large n.)
 * "err/tol" is the largest fraction of the tolerance used.
 */
#define _POSIX_C_SOURCE 200809L

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sgemm.h"

enum {
    MAX_N = 16384,
    MAX_REPS = 1000,
    FULL_CHECK_MAX_N = 256,
    SPOT_CHECKS = 64,
    FREIVALDS_VECTORS = 2,
};

/* Tolerance = TOL_SIGMAS * n * FLT_EPSILON * rms of the products. */
#define TOL_SIGMAS 4.0

static uint64_t rng_state;

static uint64_t rng_next(void)
{
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return rng_state = x;
}

/* Uniform in [-1, 1). */
static double rng_unit(void)
{
    return (double)(rng_next() >> 11) * 0x1.0p-52 - 1.0;
}

static void *xalloc(size_t bytes)
{
    void *p = NULL;
    if (posix_memalign(&p, 64, bytes) != 0) {
        fprintf(stderr, "out of memory (%zu bytes)\n", bytes);
        exit(1);
    }
    return p;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static int parse_int(const char *s, int lo, int hi, const char *what)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || v < lo || v > hi) {
        fprintf(stderr, "bad %s '%s' (want %d..%d)\n", what, s, lo, hi);
        exit(2);
    }
    return (int)v;
}

struct check {
    double worst; /* largest err / tol seen (NaN if C held a NaN) */
    int ok;
    char first_bad[128]; /* description of the first failure, printed after the row */
};

static void note(struct check *c, double err, double tol, long i, long j, double got,
                 double want)
{
    double r = tol > 0.0 ? err / tol : (err == 0.0 ? 0.0 : INFINITY);
    if (!(r <= 1.0)) { /* also true for NaN */
        c->ok = 0;
        if (c->first_bad[0] == '\0') {
            if (j >= 0)
                snprintf(c->first_bad, sizeof c->first_bad,
                         "first bad element: C[%ld][%ld] = %.9g, want %.9g", i, j, got, want);
            else
                snprintf(c->first_bad, sizeof c->first_bad,
                         "Freivalds check failed in row %ld", i);
        }
    }
    if (isnan(r) || r > c->worst)
        c->worst = r;
}

static void check_element(struct check *c, int n, const float *A, const float *B,
                          const float *C, size_t i, size_t j)
{
    const size_t N = (size_t)n;
    double exact = 0.0, sumsq = 0.0;
    for (size_t k = 0; k < N; k++) {
        double p = (double)A[i * N + k] * B[k * N + j];
        exact += p;
        sumsq += p * p;
    }
    double tol = TOL_SIGMAS * FLT_EPSILON * sqrt(n * sumsq); /* = 4 n eps rms(p) */
    double got = C[i * N + j];
    note(c, fabs(got - exact), tol, (long)i, (long)j, got, exact);
}

/*
 * Freivalds: y = C*x vs z = A*(B*x). Element errors e_ij are independent, so
 * the error of y_i is spread like sqrt(sum_j (tol_ij * x_j)^2), which
 * separates into sum_k a_ik^2 * w_k with w_k = sum_j b_kj^2 * x_j^2.
 */
static void check_freivalds(struct check *c, int n, const float *A, const float *B,
                            const float *C)
{
    const size_t N = (size_t)n;
    double *x = xalloc(N * sizeof *x);
    double *bx = xalloc(N * sizeof *bx);
    double *w = xalloc(N * sizeof *w);
    for (int v = 0; v < FREIVALDS_VECTORS; v++) {
        for (size_t j = 0; j < N; j++)
            x[j] = rng_unit();
        for (size_t k = 0; k < N; k++) {
            double s = 0.0, q = 0.0;
            for (size_t j = 0; j < N; j++) {
                double p = (double)B[k * N + j] * x[j];
                s += p;
                q += p * p;
            }
            bx[k] = s;
            w[k] = q;
        }
        for (size_t i = 0; i < N; i++) {
            double z = 0.0, var = 0.0, y = 0.0;
            for (size_t k = 0; k < N; k++) {
                double a = A[i * N + k];
                z += a * bx[k];
                var += a * a * w[k];
            }
            for (size_t j = 0; j < N; j++)
                y += (double)C[i * N + j] * x[j];
            double tol = TOL_SIGMAS * FLT_EPSILON * sqrt(n * var);
            note(c, fabs(y - z), tol, (long)i, -1, y, z);
        }
    }
    free(x);
    free(bx);
    free(w);
}

static struct check verify(int n, const float *A, const float *B, const float *C)
{
    struct check c = { 0.0, 1, "" };
    const size_t N = (size_t)n;
    if (n <= FULL_CHECK_MAX_N) {
        for (size_t i = 0; i < N; i++)
            for (size_t j = 0; j < N; j++)
                check_element(&c, n, A, B, C, i, j);
        return c;
    }
    for (int s = 0; s < SPOT_CHECKS; s++) {
        size_t i = rng_next() % N, j = rng_next() % N;
        check_element(&c, n, A, B, C, i, j);
    }
    check_freivalds(&c, n, A, B, C);
    return c;
}

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [-r reps] [-c] [n ...]\n", prog);
}

int main(int argc, char **argv)
{
    int reps = 3, csv = 0, opt;
    while ((opt = getopt(argc, argv, "r:ch")) != -1) {
        switch (opt) {
        case 'r': reps = parse_int(optarg, 1, MAX_REPS, "reps"); break;
        case 'c': csv = 1; break;
        default: usage(argv[0]); return opt == 'h' ? 0 : 2;
        }
    }

    static const int default_sizes[] = { 512, 1024, 2048 };
    int nsizes = argc - optind;
    int *sizes = xalloc((nsizes ? (size_t)nsizes : 3) * sizeof *sizes);
    if (nsizes == 0) {
        nsizes = 3;
        memcpy(sizes, default_sizes, sizeof default_sizes);
    } else {
        for (int s = 0; s < nsizes; s++)
            sizes[s] = parse_int(argv[optind + s], 1, MAX_N, "size");
    }

    const char *name = strrchr(argv[0], '/');
    name = name ? name + 1 : argv[0];
    const char *peak_env = getenv("SGEMM_PEAK_GFLOPS");
    double peak = peak_env ? strtod(peak_env, NULL) : 0.0;

    if (csv)
        printf("kernel,n,reps,best_s,median_s,gflops,pct_peak,err_over_tol,check\n");
    else
        printf("%-12s %6s %4s %10s %10s %9s %6s %9s  %s\n", "kernel", "n", "reps",
               "best_s", "median_s", "GFLOP/s", "%peak", "err/tol", "check");

    int failed = 0;
    double *t = xalloc((size_t)reps * sizeof *t);
    for (int s = 0; s < nsizes; s++) {
        int n = sizes[s];
        size_t count = (size_t)n * n;
        float *A = xalloc(count * sizeof *A);
        float *B = xalloc(count * sizeof *B);
        float *C = xalloc(count * sizeof *C);

        rng_state = 0x9E3779B97F4A7C15ull ^ (uint64_t)n;
        for (size_t i = 0; i < count; i++) {
            A[i] = (float)rng_unit();
            B[i] = (float)rng_unit();
            C[i] = NAN;
        }

        sgemm(n, A, B, C); /* warm-up: faults pages in, warms caches and clocks */
        for (int r = 0; r < reps; r++) {
            double t0 = now_s();
            sgemm(n, A, B, C);
            t[r] = now_s() - t0;
        }
        qsort(t, (size_t)reps, sizeof *t, cmp_double);
        double best = t[0];
        double median = reps % 2 ? t[reps / 2] : 0.5 * (t[reps / 2 - 1] + t[reps / 2]);
        double gflops = 2.0 * n * n * (double)n / best * 1e-9;

        struct check c = verify(n, A, B, C);
        failed |= !c.ok;

        char pct[16] = "-";
        if (peak > 0.0)
            snprintf(pct, sizeof pct, "%.1f", 100.0 * gflops / peak);
        if (csv)
            printf("%s,%d,%d,%.6f,%.6f,%.3f,%s,%.3g,%s\n", name, n, reps, best, median,
                   gflops, pct, c.worst, c.ok ? "ok" : "FAIL");
        else
            printf("%-12s %6d %4d %10.6f %10.6f %9.3f %6s %9.2g  %s\n", name, n, reps,
                   best, median, gflops, pct, c.worst, c.ok ? "ok" : "FAIL");
        fflush(stdout);
        if (!c.ok)
            fprintf(stderr, "  %s\n", c.first_bad);

        free(A);
        free(B);
        free(C);
    }
    free(t);
    free(sizes);
    return failed;
}
