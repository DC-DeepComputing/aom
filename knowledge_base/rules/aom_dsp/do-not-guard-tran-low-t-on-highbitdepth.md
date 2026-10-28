---
name: libaom/aom_dsp/do-not-guard-tran-low-t-on-highbitdepth
description: 'When loading, storing, or processing tran_low_t coefficient buffers, do not add #if CONFIG_AV1_HIGHBITDEPTH guards or 16-bit fallbacks because tran_low_t is unconditionally int32_t in libaom'
rubric: correctness
severity: should_fix
---

**Rule.** Do not conditionally guard tran_low_t buffer operations, coefficient
handling, or SIMD loads/stores with '#if CONFIG_AV1_HIGHBITDEPTH', and do not
provide 16-bit fallbacks. In libaom, tran_low_t is unconditionally defined as a
32-bit signed integer (int32_t in aom_dsp/aom_dsp_common.h) across all build
configurations, so transform and quantization coefficient processing must always
use 32-bit operations and full-width vector offsets.

**Why.** In libvpx, tran_low_t was conditionally typedef'd as int16_t or int32_t
depending on CONFIG_VP9_HIGHBITDEPTH. In libaom, tran_low_t is always int32_t
regardless of CONFIG_AV1_HIGHBITDEPTH. Guarding SIMD vector loads or stores of
tran_low_t arrays leaves memory partially unprocessed or uninitialized in 8-bit
builds, causing data corruption, while adding 16-bit branches creates dead and
broken code paths.

## Examples

Bad:

```cpp
// Variant 1: Guarding 32-bit SIMD stores leaves buffer uninitialized in 8-bit builds
_mm256_store_si256((__m256i *)(qcoeff_ptr), big_zero);
#if CONFIG_AV1_HIGHBITDEPTH
_mm256_store_si256((__m256i *)(qcoeff_ptr + 8), big_zero);
#endif  // CONFIG_AV1_HIGHBITDEPTH

// Variant 2: Adding 16-bit fallback branches for tran_low_t
#if CONFIG_AV1_HIGHBITDEPTH
  _mm_store_si128((__m128i *)(dqcoeff), dqcoeff32_0);
  _mm_store_si128((__m128i *)(dqcoeff + 4), dqcoeff32_1);
#else
  _mm_store_si128((__m128i *)(dqcoeff), dqcoeff16);
#endif  // CONFIG_AV1_HIGHBITDEPTH
```

Good:

```cpp
// Variant 1: Unconditionally store full 32-bit tran_low_t vector elements
_mm256_store_si256((__m256i *)(qcoeff_ptr), big_zero);
_mm256_store_si256((__m256i *)(qcoeff_ptr + 8), big_zero);

// Variant 2: Unconditionally process tran_low_t as 32-bit
_mm_store_si128((__m128i *)(dqcoeff), dqcoeff32_0);
_mm_store_si128((__m128i *)(dqcoeff + 4), dqcoeff32_1);
```
