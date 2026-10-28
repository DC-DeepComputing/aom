---
name: libaom/av1/avoid-redundant-dimension-reconstruction-shifts
description: When indexing base or luma buffers across subsampled planes, use existing base dimensions or strides directly instead of recomputing them via bit shifts
rubric: readability
severity: should_fix
---

**Rule.** When indexing base or luma buffers (such as frame_sse or luma_sse_sum)
in temporal filter routines or multi-plane loops, use existing known base
dimensions or strides directly (e.g., BW, BH, mb_width, mb_height, block_width,
or buffer stride). Do not define intermediate variables that reconstruct base
dimensions by bit-shifting subsampled chroma dimensions (e.g., frame_sse_stride
<< ss_x_shift, plane_w << ss_x_shift, or (base_dim >> shift) << shift).

**Why.** For standard block dimensions and subsampling ratios (4:2:0 or 4:2:2),
shifting right and then left by the subsampling factor is an exact identity
operation. Recomputing the original luma dimension or macroblock stride into
temporary variables adds redundant arithmetic, obscures data flow and canonical
buffer dimensions, and introduces redundant local aliases.

## Examples

Bad:

```cpp
// Redundantly reconstructing base dimension/stride from subsampled dimensions:
const int ww = frame_sse_stride << ss_x_shift;
for (int ii = 0; ii < (1 << ss_y_shift); ++ii) {
  for (int jj = 0; jj < (1 << ss_x_shift); ++jj) {
    const int yy = (i << ss_y_shift) + ii;
    const int xx = (j << ss_x_shift) + jj;
    luma_sse_sum[i * BW + j] += frame_sse[yy * ww + xx];
  }
}
```

Good:

```cpp
// Use known base block dimension BW or stride directly without round-trip shifting:
for (int ii = 0; ii < (1 << ss_y_shift); ++ii) {
  for (int jj = 0; jj < (1 << ss_x_shift); ++jj) {
    const int yy = (i << ss_y_shift) + ii;
    const int xx = (j << ss_x_shift) + jj;
    luma_sse_sum[i * BW + j] += frame_sse[yy * BW + xx];
  }
}
```
