/* verify_anchors.c — calibrate posit32(es=2) regime anchor bit patterns
 * Uses positenc.h (Gustafson/Sherman) — no softposit dependency       */
#include <stdio.h>
#include <math.h>
#include "positenc.h"

int main(void) {
    positenv_init(32, 2);   /* posit32(es=2), useed=16 */
    for (int k = -5; k <= 5; k++) {
        double v = pow(16.0, (double)k);
        uint32_t p = posit_from_double(v);
        printf("k=%3d  v=%.6e  bits=0x%08X\n", k, v, p);
    }
    return 0;
}
