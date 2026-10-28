---
name: libaom/aom_dsp/avoid-integer-division-in-dsp-kernels
description: When dividing by non-power-of-two constants in DSP and SIMD kernels, use multiply-and-shift helpers with reciprocal multipliers instead of runtime division
rubric: safety
severity: should_fix
---

**Rule.** In performance-critical DSP routines and SIMD kernels (such as
rectangular DC intra predictors), avoid runtime integer division (`/` or `/=`).
When dividing by non-power-of-two constants, use fixed-point multiply-and-shift
operations (such as `divide_using_multiply_shift`) with precomputed reciprocal
multipliers (e.g., `DC_MULTIPLIER_1X2` or `DC_MULTIPLIER_1X4`).

**Why.** Hardware integer division instructions have high latency and cannot be
easily vectorized compared to single-cycle multiplication and bit-shifts.
Replacing constant integer division with reciprocal multiplication and
right-shifting computes mathematically identical results with significantly
higher throughput.

## Examples

Bad:

```cpp
uint32_t sum = _mm_cvtsi128_si32(sum_above);
sum += 10;
// Anti-pattern: runtime integer division in SIMD/DSP kernel
sum /= 20;
```

Good:

```cpp
uint32_t sum = (uint32_t)_mm_cvtsi128_si32(sum_above);
sum += 10;
// Precomputed fixed-point reciprocal multiplier eliminates hardware division
sum = divide_using_multiply_shift(sum, 2, DC_MULTIPLIER_1X4);
```
