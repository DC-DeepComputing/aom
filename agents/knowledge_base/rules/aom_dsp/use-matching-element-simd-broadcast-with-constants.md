---
name: libaom/aom_dsp/use-matching-element-simd-broadcast-with-constants
description: When initializing SIMD vector constants, use element-matching broadcast intrinsics with named macros instead of packing repeated values into wider integer hex literals
rubric: readability
severity: should_fix
---

**Rule.** When initializing vector constants with repeated element values in
SIMD routines (e.g., with _mm_set1_*, _mm256_set1_*, or vdupq_n_*), use the
broadcast intrinsic matching the target element width (e.g., _mm_set1_epi8 for
8-bit, _mm_set1_epi16 for 16-bit) and pass named constants or macros. Do not
hardcode manually packed multi-element magic hex literals into wider vector
broadcast intrinsics (such as using _mm_set1_epi32(0x40404040) or
_mm_set1_epi32(0x02000200)).

**Why.** Manually packing repeated sub-elements into wider integer literals
obscures the underlying constants, introduces magic numbers, risks host
endianness issues or silent bugs if codec macros change, and reduces code
clarity. Using element-matching intrinsics with symbolic constants ensures type
safety, clarity of intent, and maintainability across platforms.

## Examples

Bad:

```cpp
// Bad: packing repeated 8-bit or 16-bit values into 32-bit vector broadcasts
const __m128i alpha_max = _mm_set1_epi32(0x40404040);  // AOM_BLEND_A64_MAX_ALPHA is 64 (0x40)
const __m128i round_offset = _mm_set1_epi32(0x02000200);  // Packed 16-bit rounding offset
```

Good:

```cpp
// Good: use element-matching broadcast intrinsics with named constants or macros
const __m128i alpha_max = _mm_set1_epi8((char)AOM_BLEND_A64_MAX_ALPHA);
const __m128i round_offset =
    _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
```
