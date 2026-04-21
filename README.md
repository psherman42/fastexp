# fastexp

Fast approximation of exp() using Schraudolph's trick (1999).

Motivated by a brilliant presentation on RISC-V RVV activation function 
optimization by Mia Chang, FAE Manager at Andes Technology, at the 
RISC-V Now! event, Santa Clara CA, April 2026.

## The Trick

IEEE FP32 stores numbers as sign + exponent + mantissa. The exponent 
field approximates log2 of your value. Reinterpreting the bit pattern 
as an integer and adding a bias offset gives a fast exp() approximation 
— no lookup table, no Taylor series, just integer arithmetic on 
floating point bits.

## Build and Run

    c++ -o fastexp fastexp.c -lm
    fastexp > fastexp.csv

## Error Profile

Tested across x = [-1.5, 1.5]: error oscillates between ~0% and ~6%, 
worst near x = ±1.1. Structured approximation — not random noise. 
Acceptable for softmax and sigmoid in neural network inference where 
relative ordering matters more than exact values.

## RISC-V RVV Connection

On RISC-V with RVV, the full pipeline collapses to three standard 
vector instructions — no proprietary extensions required:

- vfmul — scale x into integer range
- vfcvt — convert FP→Int (the Schraudolph step, vectorized)  
- vadd  — apply the bias offset across the vector

~16 cycles for 128 elements.

## Reference

N. N. Schraudolph, "A fast, compact approximation of the exponential 
function," Neural Computation, 1999.

## Author

Paul Sherman  
pauldylansherman@icloud.com  
Chair, RISC-V Academic & Training Committee  
github.com/psherman42
