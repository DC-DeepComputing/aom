---
name: libaom/aom_dsp/use-sign-mask-arithmetic-for-sse2-abs-s8
description: When computing signed 8-bit vector absolute values in SSE2 without SSSE3 intrinsics, use the three-instruction sign-mask trick rather than mask-and-blend logic
rubric: safety
severity: should_fix
---

**Rule.** When implementing signed 8-bit vector absolute value (abs_s8) for SSE2
targets where SSSE3 _mm_abs_epi8 is unavailable, compute the sign mask with
_mm_cmplt_epi8(a, _mm_setzero_si128()) and apply the two's-complement arithmetic
trick _mm_xor_si128(sign, _mm_add_epi8(a, sign)). Do not implement 8-bit
absolute values using multi-instruction negation, comparison, and bitwise
mask-and-blend (andnot/and/or) sequences.

**Why.** SSE2 lacks signed 8-bit min/max instructions (_mm_max_epi8 and
_mm_min_epi8 only exist as unsigned operations, while _mm_max_epi16 exists for
16-bit). Emulating 8-bit absolute value via negation and bitwise blending
requires five instructions (sub, cmplt, andnot, and, or), whereas the
two's-complement sign-mask trick (a + sign) ^ sign computes the exact absolute
value in three instructions (cmplt, add, xor), reducing instruction count and
register pressure in hot SIMD kernels.

## Examples

Bad:

```cpp
// 5-instruction mask-and-blend emulation
v64 t = _mm_sub_epi8(_mm_setzero_si128(), a);
v64 mask = _mm_cmplt_epi8(t, a);
return _mm_or_si128(_mm_andnot_si128(mask, t), _mm_and_si128(mask, a));
```

Good:

```cpp
// 3-instruction two's-complement sign-mask trick: (a + sign) ^ sign
v64 sign = _mm_cmplt_epi8(a, _mm_setzero_si128());
return _mm_xor_si128(sign, _mm_add_epi8(a, sign));
```
