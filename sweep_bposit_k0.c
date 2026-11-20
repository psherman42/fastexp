/*
 * sweep_bposit_k0.c — error sweep for bposit_expf_k0 over neural-net activation range
 *
 * Build (on Pioneer):
 *   gcc -x assembler-with-cpp -DRV64 -c fast_bposit_exp.s -o fast_bposit_exp.o
 *   gcc -O2 sweep_bposit_k0.c fast_bposit_exp.o -o sweep_bposit_k0 -lm
 *
 * Run:
 *   ./sweep_bposit_k0
 *   ./sweep_bposit_k0 | tee sweep_results.csv
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "fast_bposit_exp.h"

/* b-posit(32,es=2,rs=6) anchor table: k=-6..+5, plus sentinel */
static const uint32_t ANCHORS[13] = {
    0x01000000,  /* k=-6: useed^-6 = 16^-6  */
    0x02000000,  /* k=-5 */
    0x04000000,  /* k=-4 */
    0x08000000,  /* k=-3 */
    0x10000000,  /* k=-2 */
    0x20000000,  /* k=-1 */
    0x40000000,  /* k= 0: 1.0              */
    0x60000000,  /* k=+1: 16.0             */
    0x70000000,  /* k=+2 */
    0x78000000,  /* k=+3 */
    0x7C000000,  /* k=+4 */
    0x7E000000,  /* k=+5 */
    0x7FFFFFFF   /* maxpos sentinel        */
};

static float bposit_to_float(uint32_t p) {
    if (p == 0) return 0.0f;
    uint32_t abs_p = p & 0x7FFFFFFF;
    if (abs_p >= 0x7FFFFFFF) return (p & 0x80000000) ? -1e38f : 1e38f;

    /* find regime index */
    int idx = 0;
    while (idx < 11 && abs_p >= ANCHORS[idx + 1]) idx++;

    int k = idx - 6;
    float linear_frac = (float)(abs_p - ANCHORS[idx]) /
                        (float)(ANCHORS[idx + 1] - ANCHORS[idx]);
    float val = powf(16.0f, (float)k + linear_frac);
    return (p & 0x80000000) ? -val : val;
}

int main(void) {
    /* sweep range and resolution */
    float x_min  = -3.0f;
    float x_max  =  3.0f;
    float x_step =  0.01f;

    float max_abs_err  = 0.0f;
    float max_rel_err  = 0.0f;
    float sum_sq_err   = 0.0f;
    int   n_points     = 0;
    float max_err_at_x = 0.0f;

    /* CSV header */
    printf("x,ref_exp,bposit_exp,abs_err,rel_err_pct\n");

    for (float x = x_min; x <= x_max + x_step * 0.5f; x += x_step) {
        float ref   = expf(x);
        uint32_t bp = bposit_expf_k0(x);
        float approx = bposit_to_float(bp);

        float abs_err = fabsf(approx - ref);
        float rel_err = (ref > 1e-10f) ? (abs_err / ref) * 100.0f : 0.0f;

        printf("%.4f,%.8f,%.8f,%.8f,%.4f\n",
               x, ref, approx, abs_err, rel_err);

        if (rel_err > max_rel_err) {
            max_rel_err  = rel_err;
            max_err_at_x = x;
        }
        if (abs_err > max_abs_err) max_abs_err = abs_err;
        sum_sq_err += rel_err * rel_err;
        n_points++;
    }

    float rms_err = sqrtf(sum_sq_err / n_points);

    fprintf(stderr, "\n--- sweep summary: x in [%.1f, %.1f], %d points ---\n",
            x_min, x_max, n_points);
    fprintf(stderr, "  max relative error: %.4f%% at x=%.4f\n",
            max_rel_err, max_err_at_x);
    fprintf(stderr, "  max absolute error: %.6f\n", max_abs_err);
    fprintf(stderr, "  RMS relative error: %.4f%%\n", rms_err);

    /* also print key activation checkpoints */
    fprintf(stderr, "\n--- activation checkpoints ---\n");
    float checkpoints[] = {-2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f};
    int nc = sizeof(checkpoints) / sizeof(checkpoints[0]);
    for (int i = 0; i < nc; i++) {
        float xc    = checkpoints[i];
        float ref   = expf(xc);
        float approx = bposit_to_float(bposit_expf_k0(xc));
        float rel   = (ref > 1e-10f) ? fabsf(approx - ref) / ref * 100.0f : 0.0f;
        fprintf(stderr, "  exp(%.2f) = %.6f, approx = %.6f, err = %.4f%%\n",
                xc, ref, approx, rel);
    }

    return 0;
}
