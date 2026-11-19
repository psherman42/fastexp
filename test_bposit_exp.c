/* test_bposit_exp.c -- Pioneer benchmark for fast_bposit_exp (v2)
 * Build: gcc -x assembler-with-cpp -DRV64 -c fast_bposit_exp.s -o fast_bposit_exp.o
 *        gcc -O2 test_bposit_exp.c fast_bposit_exp.o -o test_bposit_exp -lm
 */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include "fast_bposit_exp.h"

/* Monotonic nanoseconds -- works on Pioneer where rdcycle is blocked */
static inline uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* Anchor table: b-posit32(es=2,rs=6) bit pattern for 16^k, k=-6..+5
 * Value range for k is [anchors[k+6], anchors[k+7]) -- linear log16 encoding */
static const uint32_t ANCHORS[13] = {
    0x01000000,  /* k=-6 */
    0x02000000,  /* k=-5 */
    0x04000000,  /* k=-4 */
    0x08000000,  /* k=-3 */
    0x10000000,  /* k=-2 */
    0x20000000,  /* k=-1 */
    0x40000000,  /* k= 0  (1.0) */
    0x60000000,  /* k=+1 */
    0x70000000,  /* k=+2 */
    0x78000000,  /* k=+3 */
    0x7C000000,  /* k=+4 */
    0x7E000000,  /* k=+5 */
    0x7FFFFFFF   /* maxpos sentinel */
};

/* Correct b-posit(32,es=2,rs=6) decoder.
 * The whole bit range [anchor_k, anchor_k+1) encodes values [16^k, 16^(k+1))
 * via linear interpolation of the log16 -- same as IEEE float's log2 trick
 * but with no bias constant and variable precision between regimes. */
static float bposit_to_float(uint32_t p) {
    if (p == 0x00000000) return 0.0f;
    if (p == 0x80000000) return 1.0f / 0.0f; /* NaR */
    int sign = (int)(p >> 31);
    uint32_t abs_p = sign ? (uint32_t)(-(int32_t)p) : p;

    /* Find regime k from leading bits of abs_p */
    int k;
    if (abs_p & 0x40000000) {         /* bit 30 = 1: positive regime */
        /* count leading ones in bits [30:25] */
        int rb = (int)((abs_p >> 25) & 0x3F);  /* 6-bit regime field */
        int cnt = 0;
        while ((rb & 0x20) && cnt < 6) { rb <<= 1; cnt++; }
        k = cnt - 1;
    } else {                           /* bit 30 = 0: negative regime */
        int rb = (int)((abs_p >> 25) & 0x3F);
        int cnt = 0;
        while (!(rb & 0x20) && cnt < 6) { rb <<= 1; cnt++; }
        k = -cnt;
    }

    int idx = k + 6;
    if (idx < 0  || abs_p <= ANCHORS[0]) return sign ? -0.0f : 0.0f;
    if (idx > 10 || abs_p >= ANCHORS[11]) {
        float big = 1.84467e19f; /* approx maxpos for es=2 */
        return sign ? -big : big;
    }

    /* Linear interpolation: fraction of the way from anchor_k to anchor_k+1
     * maps to 16^(k + frac) */
    float linear_frac = (float)(abs_p - ANCHORS[idx]) /
                        (float)(ANCHORS[idx+1] - ANCHORS[idx]);
    float value = powf(16.0f, (float)k + linear_frac);
    return sign ? -value : value;
}

int main(void) {
    /* ---- Accuracy check: k=0 and surrounding range ---- */
    printf("=== accuracy, k=-1 through k=+1 ===\n");
    printf("%-8s  %-12s  %-12s  %-12s  %s\n",
           "x", "expf_ref", "bposit_approx", "bposit_bits", "err%");

    float xs[] = {-2.5f,-2.0f,-1.5f,-1.0f,-0.5f,0.0f,0.5f,1.0f,1.5f,2.0f,2.5f};
    int n = (int)(sizeof(xs)/sizeof(xs[0]));
    float max_err = 0.0f;
    for (int i = 0; i < n; i++) {
        float ref   = expf(xs[i]);
        uint32_t bp = bposit_expf_k0(xs[i]);
        float approx = bposit_to_float(bp);
        float err   = 100.0f * fabsf(approx - ref) / ref;
        if (err > max_err) max_err = err;
        printf("%-8.2f  %-12.6f  %-12.6f  0x%08X  %.2f%%\n",
               xs[i], ref, approx, bp, err);
    }
    printf("max error: %.2f%%\n", max_err);

    /* ---- Throughput: fast_bposit_exp_k0 ---- */
    printf("\n=== throughput: bposit_expf_k0 (1M calls) ===\n");
    volatile uint32_t sink = 0;
    int M = 1000000;
    uint64_t t0 = get_ns();
    for (int i = 0; i < M; i++)
        sink ^= bposit_expf_k0((float)(i % 554 - 277) * 0.01f);
    uint64_t dt_bp = get_ns() - t0;
    printf("time: %lu ns  =>  %.2f ns/call  (sink=0x%X)\n",
           dt_bp, (double)dt_bp / M, sink);

    /* ---- Throughput: glibc expf() baseline ---- */
    printf("\n=== throughput: glibc expf() baseline (1M calls) ===\n");
    volatile float fsink = 0.0f;
    t0 = get_ns();
    for (int i = 0; i < M; i++)
        fsink += expf((float)(i % 554 - 277) * 0.01f);
    uint64_t dt_lib = get_ns() - t0;
    printf("time: %lu ns  =>  %.2f ns/call  (sink=%.1f)\n",
           dt_lib, (double)dt_lib / M, (double)fsink);

    printf("\nspeedup vs expf(): %.1fx\n", (double)dt_lib / dt_bp);
    printf("max error (k0 range): %.2f%%\n", max_err);
    return 0;
}
