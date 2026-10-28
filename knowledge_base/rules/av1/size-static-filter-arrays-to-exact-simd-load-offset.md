---
name: libaom/av1/size-static-filter-arrays-to-exact-simd-load-offset
description: When defining static filter or coefficient arrays accessed with SIMD load offsets, size them to exact maximum offset plus vector width and document the rationale
rubric: readability
severity: nit
---

**Rule.** When defining static convolution, interpolation filter, lookup table,
or coefficient arrays that are accessed at an offset by SIMD vector loads (such
as _mm_loadu_si128(filter + offset) or vld1q_s16), size the array to the exact
required capacity of maximum offset plus vector width (e.g., 9 int16_t elements
for an 8-tap vector load at offset +1). Do not allocate arbitrary extra padding
beyond the maximum loaded index, and add an explanatory comment documenting why
the array size exceeds the logical tap count to accommodate the SIMD load
offset.

**Why.** SIMD vector load intrinsics read fixed-width registers (e.g., 128-bit
loads read 8 int16_t elements), which will read past the end of an unpadded
array when passed an interior offset. Sizing the array to the exact offset plus
vector width prevents out-of-bounds reads without wasting memory or confusing
readers with arbitrary padding, while an explicit comment documents the sizing
invariant for future maintainers.

## Examples

Bad:

```cpp
// Sized arbitrarily to 10 without documenting the SIMD load offset requirement:
DECLARE_ALIGNED(16, static const int16_t, gauss_filter[10]) = { 0,  8, 30, 52,
                                                                30, 8, 0,  0 };

// Sized to 9 for SIMD load at offset +1, but missing explanatory comment:
DECLARE_ALIGNED(16, static const int16_t, sub_pel_filters[9]) = { 0,  8, 30, 52,
                                                                  30, 8, 0,  0 };
```

Good:

```cpp
// The array is of size 9 (8 taps + 1 offset) to allow passing gauss_filter + 1
// to _mm_loadu_si128() in prepare_coeffs_6t().
DECLARE_ALIGNED(16, static const int16_t, gauss_filter[9]) = { 0,  8, 30, 52,
                                                               30, 8, 0,  0 };
```
