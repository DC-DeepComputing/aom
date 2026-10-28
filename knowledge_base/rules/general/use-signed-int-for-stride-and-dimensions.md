---
name: libaom/use-signed-int-for-stride-and-dimensions
description: When declaring buffer stride, dimension, or count parameters (such as stride, rows, cols), use int rather than ptrdiff_t, uint32_t, or int32_t
rubric: readability
severity: nit
---

**Rule.** When declaring parameters or variables for buffer strides (stride,
src_stride, dst_stride), dimensions (rows, cols, width, height), and general
loop counts, use standard signed int rather than ptrdiff_t, fixed-width
(int32_t, uint32_t), or unsigned (uint32_t, size_t) integer types. When
preventing 2D buffer offset multiplication overflow, cast to ptrdiff_t at the
arithmetic site rather than declaring the stride parameter as ptrdiff_t.

**Why.** In libaom, standard signed int is the conventional type for dimensions,
counts, and buffer strides (which must support negative values for reverse
buffer traversal). Using standard int maintains uniform parameter types across
image and DSP interfaces and avoids redundant casts; 64-bit widening for large
frame offset arithmetic is handled locally via (ptrdiff_t) casts.

## Examples

Bad:

```cpp
// Anti-pattern: Using ptrdiff_t, uint32_t, or int32_t for strides and dimensions
uint8_t av1_find_dominant_value(const uint8_t *src, ptrdiff_t stride, int rows,
                                int cols);

static inline void dilate_block(const uint8_t *src, uint32_t src_stride,
                                uint8_t *dilated, uint32_t dilated_stride,
                                int32_t rows, int32_t cols);
```

Good:

```cpp
// Recommended: Standard signed int for buffer strides and dimensions
uint8_t av1_find_dominant_value(const uint8_t *src, int stride, int rows,
                                int cols);

static inline void dilate_block(const uint8_t *src, int src_stride,
                                uint8_t *dilated, int dilated_stride,
                                int rows, int cols);
```
