---
name: libaom/scale-vertical-row-offsets-by-stride
description: When indexing flattened 2D buffers or multi-row blocks, ensure vertical row increments and subsequent row offsets are scaled by buffer stride rather than added unscaled
rubric: correctness
severity: blocking
---

**Rule.** When accessing multi-row blocks (such as 2x2 or 4x4 sub-blocks) or
calculating linear offsets in flattened 2D buffers, ensure all vertical row
increments and row loop indices are multiplied by the buffer row stride (e.g.,
`(row + 1) * stride`, `output_stride * (y + 1)`, or `offset + stride`). Never
add unscaled row loop indices (such as `+ j` or `+ row`) or unscaled vertical
offsets directly to the 1D index across loop iterations or unrolled block
operations.

**Why.** In flattened 2D buffers addressed by row and column coordinates (`y *
stride + x`), advancing vertically by N rows requires incrementing the linear
offset by `N * stride` elements. Adding an unscaled loop variable or row offset
advances along the horizontal axis instead of jumping rows, reading erroneous
pixels, corrupting block data, or causing out-of-bounds memory accesses.

## Examples

Bad:

```cpp
// Incorrect: adding unscaled row index 'j' instead of multiplying by stride
comp_pred[j * comp_stride + i] =
    (2 + buffer[2 * i + 16 * 2 * j] +
     buffer[1 + 2 * i + 16 * 2 * j] +
     buffer[2 * i + 16 * 2 * j + j] +
     buffer[1 + 2 * i + 16 * 2 * j + j]) / 4;

// Incorrect: adding loop index 'j' instead of advancing by dst_stride
dst[i + j * dst_stride] = val0;
dst[i + j * dst_stride + 1] = val1;
dst[i + j * dst_stride + j] = val2;
dst[i + j * dst_stride + 1 + j] = val3;
```

Good:

```cpp
// Correct: multiplying vertical row offset (scaled_j + 1) by output_stride
const int scaled_i = 2 * i;
const int scaled_j = 2 * j;
const int output_stride = 16;
float total = output[scaled_i + output_stride * scaled_j] +
              output[scaled_i + output_stride * scaled_j + 1] +
              output[scaled_i + output_stride * (scaled_j + 1)] +
              output[scaled_i + output_stride * (scaled_j + 1) + 1] + 2.0f;
comp_pred[j * comp_stride + i] = fclamp(total / 4.0f, 0, 255);

// Correct: advancing to the next row using (j + 1) * dst_stride
dst[i + j * dst_stride] = val0;
dst[i + j * dst_stride + 1] = val1;
dst[i + (j + 1) * dst_stride] = val2;
dst[i + (j + 1) * dst_stride + 1] = val3;
```
