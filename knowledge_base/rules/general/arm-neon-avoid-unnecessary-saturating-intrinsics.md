---
name: libaom/arm-neon-avoid-unnecessary-saturating-intrinsics
description: When implementing ARM NEON vector shifts or arithmetic where bounds cannot overflow, use non-saturating intrinsics (like vrshlq_*) instead of saturating variants (vqrshlq_*)
rubric: safety
severity: should_fix
---

**Rule.** Do not use saturating ARM NEON shift or arithmetic intrinsics (such as
vqrshl[q]_*, vqshl[q]_*, vqadd[q]_*, or vqsub[q]_*) when arithmetic bounds
cannot overflow and the C reference specification does not require saturation.
Use standard or rounding non-saturating variants (such as vrshl[q]_* or
vshl[q]_*) instead.

**Why.** Saturating SIMD instructions incur extra hardware logic and potential
latency overhead to detect and clamp overflow conditions. When values are
mathematically guaranteed within range (e.g., in normalized weighted averaging
or bounded variance calculations), non-saturating intrinsics provide identical
results with higher throughput.

## Examples

Bad:

```cpp
// Using saturating rounding shift when values cannot overflow bounds
const int32x4_t shift = vdupq_n_s32(-shift_bits);
cp0 = vqrshlq_u32(cp0, shift);
cp1 = vqrshlq_u32(cp1, shift);
```

Good:

```cpp
// Using non-saturating rounding shift
const int32x4_t shift = vdupq_n_s32(-shift_bits);
cp0 = vrshlq_u32(cp0, shift);
cp1 = vrshlq_u32(cp1, shift);
```
