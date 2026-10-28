---
name: libaom/av1/validate-dimensions-individually-for-coordinate-offsets
description: When checking frame or image dimensions before computing spatial offsets or centering sub-windows, validate width and height individually rather than aggregate area
rubric: correctness
severity: blocking
---

**Rule.** When validating frame or image dimensions before calculating spatial
coordinate offsets (such as centering a fixed-size block or extracting a
sub-window), check width and height individually (width >= min_w && height >=
min_h) rather than aggregate pixel area (width * height >= min_area).

**Why.** An aggregate area check can be satisfied by extreme aspect ratios where
one dimension is smaller than the required block or sub-window size. This causes
negative coordinate offsets or out-of-bounds pointer arithmetic when indexing
frame buffers.

## Examples

Bad:

```cpp
// Flawed: extreme aspect ratio (e.g. 10000x20) satisfies area but causes negative row offset
if (unscaled_src->y_width * unscaled_src->y_height >= 1280 * 720) {
  int pos_col = (unscaled_src->y_width >> 1) - 64;
  int pos_row = (unscaled_src->y_height >> 1) - 64;
  const uint8_t *src_y =
      unscaled_src->y_buffer + pos_row * src_ystride + pos_col;
  ...
}
```

Good:

```cpp
// Safe: ensures both dimensions independently accommodate the centered 128x128 block
if (unscaled_src->y_width >= 1280 && unscaled_src->y_height >= 720) {
  int pos_col = (unscaled_src->y_width >> 1) - 64;
  int pos_row = (unscaled_src->y_height >> 1) - 64;
  const uint8_t *src_y =
      unscaled_src->y_buffer + pos_row * src_ystride + pos_col;
  ...
}
```
