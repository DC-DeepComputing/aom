---
name: libaom/aom_dsp/prefer-loadu-over-lddqu-simd-intrinsics
description: When performing unaligned vector loads in x86 SIMD code, prefer standard _mm_loadu_si128 or _mm256_loadu_si256 over legacy _mm_lddqu_si128 or _mm256_lddqu_si256
rubric: readability
severity: should_fix
---

**Rule.** In x86 SIMD intrinsic functions (such as SSE3, SSSE3, SSE4.1, AVX2,
and AVX-512 routines), do not use legacy lddqu intrinsics (_mm_lddqu_si128,
_mm256_lddqu_si256) for unaligned vector loads; use standard unaligned load
intrinsics (_mm_loadu_si128, _mm256_loadu_si256).

**Why.** _mm_lddqu_si128 was introduced on early microarchitectures (such as
Pentium 4 / NetBurst) to avoid cache-line split penalties, but modern x86
processors handle unaligned loads efficiently without special instructions.
Using _mm_loadu_si128 and _mm256_loadu_si256 follows standard libaom SIMD
conventions and keeps intrinsic usage consistent across x86 instruction sets.

## Examples

Bad:

```cpp
// Legacy lddqu intrinsic for unaligned load
const __m128i src_vec = _mm_lddqu_si128((const __m128i *)(src_ptr + src_stride * 7));
const __m256i block_vec = _mm256_lddqu_si256((const __m256i *)src_ptr);
```

Good:

```cpp
// Standard unaligned load intrinsic
const __m128i src_vec = _mm_loadu_si128((const __m128i *)(src_ptr + src_stride * 7));
const __m256i block_vec = _mm256_loadu_si256((const __m256i *)src_ptr);
```
