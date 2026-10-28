---
name: libaom/aom_dsp/advance-destination-pointer-by-total-packed-elements-in-simd
description: When packing and storing multiple rows into a contiguous buffer in SIMD loops, advance the destination pointer by the total elements stored rather than single-row width
rubric: correctness
severity: blocking
---

**Rule.** When packing and storing multiple rows of pixels into a contiguous
destination buffer within a SIMD loop (such as combining two 4-pixel rows into a
single 8-pixel vector), increment the destination buffer pointer by the total
number of elements written across all packed rows rather than by a single row
width.

**Why.** Storing a packed vector containing multiple rows writes num_rows *
row_width elements to the destination buffer in each iteration. Advancing the
destination pointer by only row_width causes subsequent iterations to overwrite
previously stored rows and results in buffer corruption or incorrect
predictions.

## Examples

Bad:

```cpp
/* Read 4 pixels across two rows and pack into one 128-bit vector. */
for (i = 0; i < height; i += 2) {
  __m128i s0 = _mm_loadl_epi64((const __m128i *)ref);
  __m128i s1 = _mm_loadl_epi64((const __m128i *)(ref + ref_stride));
  __m128i t0 = _mm_unpacklo_epi64(s0, s1);
  _mm_storeu_si128((__m128i *)comp_pred, t0);
  comp_pred += 4;  // Bug: advances by 1 row (4 pixels) instead of 2 rows (8 pixels).
  ref += 2 * ref_stride;
}
```

Good:

```cpp
/* Read 4 pixels across two rows and pack into one 128-bit vector. */
for (i = 0; i < height; i += 2) {
  __m128i s0 = _mm_loadl_epi64((const __m128i *)ref);
  __m128i s1 = _mm_loadl_epi64((const __m128i *)(ref + ref_stride));
  __m128i t0 = _mm_unpacklo_epi64(s0, s1);
  _mm_storeu_si128((__m128i *)comp_pred, t0);
  comp_pred += 8;  // Correct: advances by total packed elements (2 * 4 = 8 pixels).
  ref += 2 * ref_stride;
}
```
