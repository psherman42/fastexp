/* fast_bposit_exp.h -- b-posit(32,es=2,rs=6) Schraudolph fastexp
 * Link with: gcc -DRV64 fast_bposit_exp.s your_code.c -lm
 */
#ifndef FAST_BPOSIT_EXP_H
#define FAST_BPOSIT_EXP_H
#include <stdint.h>

/* Assembly entry points -- x as Q27 signed int (x_real * 2^27) */
extern uint32_t fast_bposit_exp_k0(int32_t x_q27); /* 6 instr rv64, 9 rv32 */
extern uint32_t fast_bposit_exp(int32_t x_q27);    /* full range, ~12/17 instr */

/* Convenience wrappers for float input */
static inline int32_t q27(float x)  { return (int32_t)(x * 134217728.0f); }
static inline float   q27f(int32_t x) { return (float)x * 7.4505806e-9f; }

static inline uint32_t bposit_expf_k0(float x) { return fast_bposit_exp_k0(q27(x)); }
static inline uint32_t bposit_expf(float x)    { return fast_bposit_exp(q27(x)); }
#endif
