---
name: libaom/step-loop-indices-by-block-dimensions
description: When scaling, upsampling, or unrolling loops that write multi-pixel blocks per iteration, step loop index variables by block dimensions rather than incrementing by one
rubric: correctness
severity: should_fix
---

**Rule.** When upsampling, scaling, or processing 2D image buffers where each
loop iteration reads or writes a multi-pixel block (such as 2x2 or 4x4
sub-blocks), increment loop index variables by the block step size (e.g., i +=
2, j += 2, or i += step) rather than using unit increments (++i, ++j). Ensure
destination array indexing uses the appropriate stride and offset for each pixel
in the block.

**Why.** Using unit increments (++i, ++j) in loops that write multi-pixel blocks
causes subsequent iterations to overwrite already computed destination pixels
and causes out-of-bounds memory writes past buffer boundaries on trailing
iterations.

## Examples

Bad:

```cpp
for (int j = 0; j < height; ++j) {
  for (int i = 0; i < width; ++i) {
    int src_x = i / 2;
    int src_y = j / 2;
    dst[i + j * dst_stride] = pred[src_x + src_y * src_stride];
    dst[i + 1 + j * dst_stride] = pred[src_x + src_y * src_stride];
    dst[i + (j + 1) * dst_stride] = pred[src_x + src_y * src_stride];
    dst[i + 1 + (j + 1) * dst_stride] = pred[src_x + src_y * src_stride];
  }
}
```

Good:

```cpp
for (int j = 0; j < height; j += 2) {
  for (int i = 0; i < width; i += 2) {
    int src_x = i / 2;
    int src_y = j / 2;
    dst[i + j * dst_stride] = pred[src_x + src_y * src_stride];
    dst[i + 1 + j * dst_stride] = pred[src_x + src_y * src_stride];
    dst[i + (j + 1) * dst_stride] = pred[src_x + src_y * src_stride];
    dst[i + 1 + (j + 1) * dst_stride] = pred[src_x + src_y * src_stride];
  }
}
```
