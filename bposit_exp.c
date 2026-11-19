/* test_bposit_exp.c */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "fast_bposit_exp.h"

static inline uint64_t rdcycle(void) {
    uint64_t c; __asm__ volatile("rdcycle %0" : "=r"(c)); return c;
}

int main(void) {
    float xs[] = {-2.5, -1.0, -0.5, 0.0, 0.5, 1.0, 1.5, 2.0, 2.5};
    int n = sizeof(xs)/sizeof(xs[0]);

    printf("x\t\texp_ref\t\tbposit_bits\tcycles\t\terr%%\n");
    for (int i = 0; i < n; i++) {
        float ref = expf(xs[i]);
        uint64_t t0 = rdcycle();
        uint32_t bp = bposit_expf_k0(xs[i]);
        uint64_t dt = rdcycle() - t0;
        /* decode b-posit32 bits back to float for error check */
        /* k=0: value ~ 2^exp * (1 + frac/2^23), exp=bits[24:23], frac=bits[22:0] */
        int exp2 = (bp >> 23) & 3;
        float frac = (float)(bp & 0x7FFFFF) / 8388608.0f;
        float approx = (1 << exp2) * (1.0f + frac);
        printf("%.2f\t\t%.6f\t0x%08X\t%lu\t\t%.2f%%\n",
               xs[i], ref, bp, dt, 100.0f*fabsf(approx-ref)/ref);
    }

    /* Throughput: 1M calls */
    volatile uint32_t sink = 0;
    uint64_t t0 = rdcycle();
    for (int i = 0; i < 1000000; i++)
        sink ^= bposit_expf_k0((float)(i%1000 - 500) * 0.005f);
    uint64_t total = rdcycle() - t0;
    printf("\n1M calls: %lu cycles total, %.1f cycles/call\n", total, (double)total/1e6);
    printf("(sink=%u to prevent DCE)\n", sink);
    return 0;
}
