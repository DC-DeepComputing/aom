---
name: libaom/use-column-index-not-buffer-offset-for-subsampling-parity
description: When checking horizontal subsampling parity or chroma steps in 2D image buffer loops, test the column index rather than 1D buffer offsets
rubric: correctness
severity: nit
---

**Rule.** Do not use a cumulative 1D buffer offset or pixel index (such as
y_index % 2 or offset & 1) to determine horizontal subsampling boundaries or
chroma step intervals during 2D buffer traversal. Use the column loop index or
horizontal coordinate (such as j % 2 or x & 1) instead, as cumulative 1D offsets
shift parity across row boundaries whenever the row stride is odd or padded.

**Why.** In 2D image buffers, row strides often include alignment padding or odd
byte widths. When a 1D offset is computed across rows, checking parity against
the 1D index causes the odd/even phase to invert on odd rows if the stride is
odd, corrupting chroma alignment.

## Examples

Bad:

```cpp
// y_index is incremented across rows or initialized as i * row_stride_y.
// If row_stride_y is odd, parity flips on alternating rows.
for (int i = 0; i < height; ++i) {
  for (int j = 0; j < width; ++j) {
    if (y_index % 2 == 1) {
      uv_index++;
    }
    y_index++;
  }
}
```

Good:

```cpp
// Column loop index j (0 <= j < width) guarantees consistent horizontal parity
// regardless of row stride padding or alignment.
for (int i = 0; i < height; ++i) {
  for (int j = 0; j < width; ++j) {
    if (j % 2 == 1) {
      uv_index++;
    }
    y_index++;
  }
}
```
