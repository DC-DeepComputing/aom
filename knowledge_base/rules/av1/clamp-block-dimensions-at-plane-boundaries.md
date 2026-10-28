---
name: libaom/av1/clamp-block-dimensions-at-plane-boundaries
description: When scaling or filtering image planes in fixed-size blocks, clamp boundary block dimensions with AOMMIN to prevent out-of-bounds buffer access
rubric: safety
severity: blocking
---

**Rule.** When processing, scaling, or filtering image planes in fixed-size
blocks or tiles (such as 16x16 or 8x8), clamp the working block width and height
at plane boundaries using AOMMIN(block_size, dst_dim - pos). For non-aligned
boundary blocks, dispatch to variable-dimension kernels (such as scalar _c
fallbacks) if optimized SIMD implementations assume fixed full-block geometry.

**Why.** Image plane dimensions (such as subsampled chroma planes or
non-multiple-of-16 cropped frames) often do not align with fixed block
dimensions. Processing un-clamped full-sized blocks at boundaries reads or
writes beyond buffer allocations, causing buffer overflows and memory safety
violations.

## Examples

Bad:

```cpp
// Bad: Unconditionally processes fixed 16x16 blocks at plane boundaries.
for (int y = 0; y < dst_h; y += 16) {
  for (int x = 0; x < dst_w; x += 16) {
    aom_scaled_2d(src_ptr, src_stride, dst_ptr, dst_stride, kernel,
                  x_q4 & 0xf, 16 * src_w / dst_w, y_q4 & 0xf,
                  16 * src_h / dst_h, 16, 16);
  }
}
```

Good:

```cpp
// Good: Clamps working dimensions at boundaries and uses scalar C fallback.
for (int y = 0; y < dst_h; y += 16) {
  for (int x = 0; x < dst_w; x += 16) {
    const int work_w = AOMMIN(16, dst_w - x);
    const int work_h = AOMMIN(16, dst_h - y);
    if (work_w != 16 || work_h != 16) {
      aom_scaled_2d_c(src_ptr, src_stride, dst_ptr, dst_stride, kernel,
                      x_q4 & 0xf, 16 * src_w / dst_w, y_q4 & 0xf,
                      16 * src_h / dst_h, work_w, work_h);
    } else {
      aom_scaled_2d(src_ptr, src_stride, dst_ptr, dst_stride, kernel,
                    x_q4 & 0xf, 16 * src_w / dst_w, y_q4 & 0xf,
                    16 * src_h / dst_h, 16, 16);
    }
  }
}
```
