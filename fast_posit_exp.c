/* fast_posit_exp.c -- Schraudolph-style fastexp for posit32(es=2)
 *
 * Motivation: Mia Chang (Andes Technology), RISC-V Now! Santa Clara, Apr 2026
 *   Three-instruction RVV pipeline exploiting fixed IEEE exponent field position.
 *   Ref: N. Schraudolph, "A fast, compact approximation of the exponential
 *   function," Neural Computation 11(4), 1999.
 *
 * The posit angle:
 *   IEEE Schraudolph uses two artifact constants:
 *     scale  = 1<<23        (mantissa field width -- IEEE artifact)
 *     offset = 127<<23      (exponent bias       -- IEEE artifact)
 *
 *   Posit32(es=2) needs neither:
 *     scale  = 1<<27        (regime geometry at k=0, derived from posit structure)
 *     offset = 0x40000000   (posit32 encoding of 1.0 itself -- no bias)
 *
 *   The regime field is a natural exponent encoder. Within regime k, the
 *   posit bit pattern is linear in log2(value), with a scale that halves
 *   each step away from k=0 (tapered precision made structurally explicit).
 *
 * Anchor table (ground-truthed via verify_anchors.c on got-milk):
 *   k=-5: 0x02000000    k=0: 0x40000000    k=3: 0x78000000
 *   k=-4: 0x04000000    k=1: 0x60000000    k=4: 0x7C000000
 *   k=-3: 0x08000000    k=2: 0x70000000    k=5: 0x7E000000
 *   k=-2: 0x10000000
 *   k=-1: 0x20000000
 *
 * Formula:
 *   k >= 0: anchor(k) = 0x80000000 - (1 << (30-k))
 *   k <  0: anchor(k) = 1 << (30+k)
 *
 *   scale_k = 1<<(27-k) for k>=0
 *           = 1<<(28+k) for k<0
 *
 * Target: fe310 / got-milk (RV32I softposit via positenc.h)
 *
 * Paul Sherman + Claude (IDC), RISC-V Open Lab, San Jose CA, Nov 2026
 */

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "positenc.h"

#ifndef M_LN2
#define M_LN2  0.6931471805599453
#endif

#define LN_USEED  (4.0 * M_LN2)   /* ln(16): ln of useed for posit32(es=2) */

/* ------------------------------------------------------------------ */
/* Regime anchor: bit pattern for exact value useed^k = 16^k           */

static inline uint32_t p32_anchor(int k) {
    if (k >=  29) return 0x7FFFFFFFu;   /* maxpos */
    if (k <= -30) return 0x00000001u;   /* minpos */
    if (k >= 0)   return 0x80000000u - (1u << (30 - k));
    else          return (1u << (30 + k));
}

/* Per-octave (factor-of-2) step count within regime k.
 * Halves each step away from k=0 -- tapered precision.             */

static inline double p32_octave_scale(int k) {
    int shift = (k >= 0) ? (27 - k) : (28 + k);
    if (shift <= 0) return 1.0;
    return (double)(1u << shift);
}

/* ------------------------------------------------------------------ */
/* fast_posit_exp_k0: k=0 regime only, x in [0, ln(16)) ~ [0, 2.773)
 *
 * Direct structural analog to Schraudolph:
 *   IEEE:  i = (int)(x * (1<<23)/ln2) + 127*(1<<23)
 *   Posit: p = (int)(x * (1<<27)/ln2) + 0x40000000
 *
 * No bias constant. The offset IS posit32(1.0).                     */

uint32_t fast_posit_exp_k0(double x) {
    int32_t delta  = (int32_t)(x * p32_octave_scale(0) / M_LN2);
    int32_t anchor = (int32_t)p32_anchor(0);
    return (uint32_t)(anchor + delta);
}

/* ------------------------------------------------------------------ */
/* fast_posit_exp: regime-aware, covers full float32 exp() range.
 *
 * Within each regime k, posit32 behaves as a fixed-width log encoder.
 * The piecewise structure is physically meaningful: each regime boundary
 * is an exact power of useed, not an artifact of the encoding.       */

uint32_t fast_posit_exp(double x) {
    /* determine regime k: exp(x) lies in [useed^k, useed^(k+1)) */
    int k = (int)(x / LN_USEED);
    if (x < 0.0 && (double)k * LN_USEED != x) k--;   /* floor for negatives */

    if (k >=  29) return 0x7FFFFFFFu;
    if (k <= -30) return 0x00000001u;

    double x_loc = x - (double)k * LN_USEED;          /* offset within regime */
    int32_t delta  = (int32_t)(x_loc * p32_octave_scale(k) / M_LN2);
    int32_t anchor = (int32_t)p32_anchor(k);
    return (uint32_t)(anchor + delta);
}

/* ------------------------------------------------------------------ */
/* Reference and error profile                                          */

int main(void) {
    int i;
    double x, ref_d, fast_d, fast_k0_d, err_full, err_k0;
    uint32_t ref_p, fast_p, fast_k0_p;

    positenv_init(32, 2);

    /* --- anchor self-check ---------------------------------------- */
    printf("=== anchor self-check ===\n");
    printf("%-4s  %-14s  %-12s  %-12s  %s\n",
           "k", "16^k", "formula", "verify", "match");
    {
        int k;
        for (k = -5; k <= 5; k++) {
            uint32_t formula = p32_anchor(k);
            uint32_t exact   = posit_from_double(pow(16.0, (double)k));
            printf("%-4d  %-14.6e  0x%08X  0x%08X  %s\n",
                   k, pow(16.0,(double)k), formula, exact,
                   formula == exact ? "OK" : "MISMATCH");
        }
    }

    /* --- error profile CSV ---------------------------------------- */
    printf("\n=== error profile ===\n");
    printf("x,ref_exact,ref_p,fast_p,fast_k0_p,err_full_rel,err_k0_rel\n");

    /* k=0 range [0, ln(16)) -- direct comparison with k0 version */
    for (i = 0; i <= 277; i++) {
        x = i * 0.01;
        ref_d     = exp(x);
        ref_p     = posit_from_double(ref_d);
        fast_p    = fast_posit_exp(x);
        fast_k0_p = fast_posit_exp_k0(x);
        fast_d    = posit_to_double(fast_p);
        fast_k0_d = posit_to_double(fast_k0_p);
        err_full  = (fast_d    - ref_d) / ref_d;
        err_k0    = (fast_k0_d - ref_d) / ref_d;
        printf("%.4f,%.8e,0x%08X,0x%08X,0x%08X,%+.6f,%+.6f\n",
               x, ref_d, ref_p, fast_p, fast_k0_p, err_full, err_k0);
    }

    /* multi-regime range [-4*ln(16), 4*ln(16)] -- regime-aware only */
    for (i = -444; i <= 444; i++) {
        x = i * 0.025;
        if (x >= 0.0 && x <= 2.77) continue;   /* already covered above */
        ref_d  = exp(x);
        ref_p  = posit_from_double(ref_d);
        fast_p = fast_posit_exp(x);
        fast_d = posit_to_double(fast_p);
        err_full = (fast_d - ref_d) / ref_d;
        printf("%.4f,%.8e,0x%08X,0x%08X,0x%08X,%+.6f,NA\n",
               x, ref_d, ref_p, fast_p, 0u, err_full);
    }

    return 0;
}

