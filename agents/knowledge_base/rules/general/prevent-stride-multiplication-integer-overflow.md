---
name: libaom/prevent-stride-multiplication-integer-overflow
description: Use ptrdiff_t consistently for buffer offsets and stride calculations, or advance row pointers, to prevent 32-bit integer overflow.
rubric: safety
severity: should_fix
---

**Rule.** When computing 2D buffer element addresses or linear offsets using row
indices and strides (such as src[r * stride + c] or src + r * stride),
explicitly cast the stride or row index to ptrdiff_t, or maintain advancing row
pointers (row_ptr += stride), rather than performing unchecked 32-bit integer
multiplication. Additionally, declare local and intermediate buffer offset
variables consistently as ptrdiff_t rather than mixing int and ptrdiff_t.

**Why.** Multiplying 32-bit row indices by strides can overflow signed 32-bit
integer limits on large frames or high-resolution images, causing undefined
behavior and out-of-bounds memory access. Casting to ptrdiff_t performs 64-bit
arithmetic on 64-bit systems, while advancing row pointers avoids repeated
multiplication entirely. Using ptrdiff_t consistently for all buffer and pointer
offset variables avoids sign-extension hazards and eliminates ambiguity about
whether intermediate offsets can overflow.

## Examples

Bad:

```cpp
// Variant 1: Unchecked 32-bit stride multiplication in 2D buffer indexing
for (int r = 0; r < rows; ++r) {
  for (int c = 0; c < cols; ++c) {
    const uint8_t value = src[r * stride + c];
    dst[r * dst_stride + c] = value;
  }
}

// Variant 2: Mixing int and ptrdiff_t for buffer offset variables
const ptrdiff_t buf_offset = (ptrdiff_t)(row + by_pos) * ref_stride + col + bx_pos;
const int tmp_buf_offset = by_pos * (1 << MAX_SB_SIZE_LOG2) + bx_pos;
aom_sse(&ref_buffer[buf_offset], ref_stride, &tmp_dst8[tmp_buf_offset], tmp_stride);
```

Good:

```cpp
// Variant 1: Cast stride to ptrdiff_t or advance row pointers
for (int r = 0; r < rows; ++r) {
  for (int c = 0; c < cols; ++c) {
    const uint8_t value = src[r * (ptrdiff_t)stride + c];
    dst[r * (ptrdiff_t)dst_stride + c] = value;
  }
}

// Alternatively: advance row pointers to avoid multiplication
const uint8_t *src_row = src;
uint8_t *dst_row = dst;
for (int r = 0; r < rows; ++r) {
  for (int c = 0; c < cols; ++c) {
    dst_row[c] = src_row[c];
  }
  src_row += stride;
  dst_row += dst_stride;
}

// Variant 2: Use ptrdiff_t consistently for all buffer offsets
const ptrdiff_t buf_offset = (ptrdiff_t)(row + by_pos) * ref_stride + col + bx_pos;
const ptrdiff_t tmp_buf_offset = (ptrdiff_t)by_pos * (1 << MAX_SB_SIZE_LOG2) + bx_pos;
aom_sse(&ref_buffer[buf_offset], ref_stride, &tmp_dst8[tmp_buf_offset], tmp_stride);
```
