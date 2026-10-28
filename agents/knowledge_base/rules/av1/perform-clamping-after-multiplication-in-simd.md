---
name: libaom/av1/perform-clamping-after-multiplication-in-simd
description: When implementing clamped arithmetic formulas in SIMD kernels, compute multiplications in wider intermediate precision before clamping or packing rather than saturating operands prematurely
rubric: correctness
severity: should_fix
---

**Rule.** In SIMD kernels implementing arithmetic formulas with clamping (such
as `clamp(scale * (src1 - src2), min, max)`), evaluate multiplications and
differences in wider intermediate precision (such as 32-bit integers) before
applying saturation or packing (`_mm_packs_epi32` or ARM `vqmovn`). Do not
perform narrow 16-bit multiplications on early-saturated operands (e.g.
`_mm_mullo_epi16` on `_mm_subs_epi16`), which can overflow 16-bit ranges before
clamping and produce outputs that diverge from C reference implementations.

**Why.** When scaling pixel differences or filtered terms, intermediate products
can exceed the 16-bit integer range [-32768, 32767] before final clamping.
Multiplying in 16-bit precision wraps around and discards upper bits prior to
saturation, whereas computing 32-bit products and packing with signed saturation
preserves full intermediate precision and guarantees bit-identical results with
scalar reference code.

## Examples

Bad:

```cpp
// Flawed: Subtracting with 16-bit saturation and multiplying in 16-bit precision
// overflows before clamping when the scaled difference exceeds int16 range.
const __m128i diff = _mm_subs_epi16(src1, src2);
const __m128i result = _mm_mullo_epi16(diff, scale);
```

Good:

```cpp
// Correct: Unpack or compute products in 32-bit intermediate precision,
// then clamp and pack down with signed saturation.
const __m128i diff = _mm_sub_epi16(src1, src2);
const __m128i prod_lo = _mm_mullo_epi32(_mm_cvtepi16_epi32(diff), scale_lo);
const __m128i prod_hi = _mm_mullo_epi32(_mm_cvtepi16_epi32(_mm_srli_si128(diff, 8)), scale_hi);
const __m128i result = _mm_packs_epi32(prod_lo, prod_hi);
```
