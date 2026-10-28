---
name: libaom/aom_dsp/derive-sub-block-pointers-from-offset-start-pointer
description: When computing sub-block or multi-row pointers in coordinate-indexed DSP functions, derive subsequent pointers from the coordinate-offset start pointer rather than the unindexed base buffer
rubric: correctness
severity: should_fix
---

**Rule.** In DSP, SIMD, and block helper functions that accept coordinate or
sub-block offsets (such as x_idx, y_idx, or x16_idx, y16_idx), derive pointers
for subsequent sub-blocks or rows from the already-offset region pointer (e.g.,
s_y0 + 8 * stride) rather than the unindexed base buffer pointer (s + 8 *
stride).

**Why.** Computing subsequent row or sub-block addresses relative to the raw
base buffer pointer accidentally drops the coordinate offsets (x_idx and y_idx *
stride), reading or writing data from the wrong spatial coordinates and
corrupting sub-block computations.

## Examples

Bad:

```cpp
void aom_avg_8x8_quad_avx2(const uint8_t *s, int p, int x16_idx, int y16_idx,
                           int *avg) {
  // Bad: s_y1 drops x16_idx and y16_idx by offsetting from raw base pointer 's'
  const uint8_t *s_y0 = s + y16_idx * p + x16_idx;
  const uint8_t *s_y1 = s + 8 * p;
  ...
}
```

Good:

```cpp
void aom_avg_8x8_quad_avx2(const uint8_t *s, int p, int x16_idx, int y16_idx,
                           int *avg) {
  // Good: s_y1 preserves coordinate offsets by deriving from s_y0
  const uint8_t *s_y0 = s + y16_idx * p + x16_idx;
  const uint8_t *s_y1 = s_y0 + 8 * p;
  ...
}
```
