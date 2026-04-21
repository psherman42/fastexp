// fastexp.c -- Schraudolph's trick, as presented by Mia Chang (Andes Technology)
// RISC-V Now! event, April 2026, Santa Clara CA
// Benchmarked by Paul Sherman, pauldylansherman@icloud.com
// github.com/psherman42/fastexp
//
// Build: c++ -o fastexp fastexp.c -lm
// Run: fastexp > fastexp.csv

#include <stdio.h>
#include <math.h>  // for expf()

float fast_exp (float x) {
  union { float f; int i; } u;
  u.i = (int) (12102203.0f * x) + 1065353216;
  return u.f;
}

int main (int argc, char *argv[]) {
  float ya, yb;

  printf("%s,%s,%s,%s", "\"X\"", "\"EXPF\"", "\"FAST_EXP\"", "\"ERRPCNT\"");

  for (float x=-1.5f; x<1.5f; x+=0.05) {
    ya = expf( x );
    yb = fast_exp( x );

    printf("\n%f", x);
    printf(",%f", ya);
    printf(",%f", yb);
    printf(",%f", 100.0f*(ya - yb)/ya);
  }

  printf("\n");

  return 0;
}

