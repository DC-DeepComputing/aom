---
name: libaom/consistent-variable-naming-across-simd-and-c
description: When implementing or updating SIMD kernels and C reference functions, maintain consistent variable names across architecture variants and ensure identifiers accurately reflect transformations like abs()
rubric: readability
severity: nit
---

**Rule.** Maintain consistent variable names for corresponding intermediate
computations, accumulators, and outputs across generic C reference
implementations and all architecture-specific SIMD variants (x86, ARM NEON/SVE,
RISC-V). When an intermediate value undergoes a mathematical transformation
(such as `abs()`, squaring, or scaling), give the variable an explicit
descriptor (e.g., `mean_abs` rather than `mean`) and ensure accompanying
comments match the updated identifier.

**Why.** Consistent variable naming across generic C and optimized SIMD
implementations makes routines easy to cross-reference, audit, and maintain
during algorithm updates or bug fixes. Explicitly naming transformed
intermediate values prevents ambiguity about variable contents and keeps dynamic
range comments synchronized with the actual calculations.

## Examples

Bad:

```cpp
// (mean * mean): dynamic range 32 (unsigned) bits.
const uint32_t mean = abs(horizontal_add_s16x8(v_mean));
return sse - ((mean * mean) >> (bwl + 2));
```

Good:

```cpp
// (mean_abs * mean_abs): dynamic range 32 (unsigned) bits.
const uint32_t mean_abs = abs(horizontal_add_s16x8(v_mean));
return sse - ((mean_abs * mean_abs) >> (bwl + 2));
```
