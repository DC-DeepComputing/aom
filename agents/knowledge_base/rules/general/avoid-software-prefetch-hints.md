---
name: libaom/avoid-software-prefetch-hints
description: When writing SIMD or DSP optimizations, do not add manual software prefetch hints (such as __builtin_prefetch or _mm_prefetch) without microarchitectural benchmark evidence
rubric: safety
severity: should_fix
---

**Rule.** Do not add manual software prefetch instructions or hints (such as
__builtin_prefetch or _mm_prefetch) in new SIMD, DSP, or architecture-specific
optimization routines without concrete benchmark measurements demonstrating a
speedup on target CPU microarchitectures. Rely on hardware prefetchers for
regular memory access patterns.

**Why.** Modern CPU hardware prefetchers (such as on ARM Neoverse/Cortex and x86
cores) efficiently detect and prefetch sequential or strided memory access
streams. Explicit software prefetch instructions add instruction overhead,
increase register pressure, and can cause cache thrashing or contention,
frequently regressing or failing to improve performance.

## Examples

Bad:

```cpp
// Adding manual software prefetch hints in SIMD routine
__builtin_prefetch(dst + 0 * dst_stride);
__builtin_prefetch(src + 0 * src_stride);
d0 = highbd_convolve8_4_s32_s16(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                zero_s32);
```

Good:

```cpp
// Relying on CPU hardware prefetchers without software prefetch overhead
d0 = highbd_convolve8_4_s32_s16(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                vdupq_n_s32(0));
```
