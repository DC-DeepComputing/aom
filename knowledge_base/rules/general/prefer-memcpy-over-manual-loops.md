---
name: libaom/prefer-memcpy-over-manual-loops
description: When copying or zero-initializing contiguous arrays, buffer slices, or 2D block rows, use memcpy, memset, or std::copy instead of manual element-by-element loops
rubric: readability
severity: nit
---

**Rule.** When copying, zeroing, or initializing contiguous array elements,
buffer slices, or 2D image/block rows, use bulk memory operations (memcpy,
memset) or standard C++ algorithms (std::copy, std::copy_n, std::fill) instead
of manual element-by-element assignment loops or pointer-increment loops. When
using memcpy or memset, compute byte sizes explicitly with sizeof(*dst) or
sizeof(element_type).

**Why.** Standard bulk memory operations and standard copy algorithms express
intent concisely, enable compiler and library SIMD optimizations, avoid scalar
loop overhead, and safely handle element copying and type conversions.

## Examples

Bad:

```cpp
// Manual copying across contiguous array elements
for (int c = 0; c < config->out_channels; ++c) {
  dst_buf[c] = src_buf[c];
}

// Manual element-by-element column zeroing in a 2D block row
for (int r = 0; r < mi_high; ++r) {
  for (int c = 0; c < mi_wide; ++c) {
    xd->is_mi_coded[(mi_row + r) * stride + mi_col + c] = 0;
  }
}

// Manual element copy loop in C++
for (int j = 0; j < bh + BORDER; ++j) {
  for (int i = 0; i < bw + BORDER; ++i) {
    inter_input[i + j * (bw + BORDER)] = inter_pred[i + j * stride];
  }
}
```

Good:

```cpp
// Bulk copy with memcpy and explicit byte sizing
memcpy(dst_buf, src_buf, config->out_channels * sizeof(*dst_buf));

// Bulk row zeroing with memset per contiguous row slice
for (int r = 0; r < mi_high; ++r) {
  uint8_t *row_ptr = xd->is_mi_coded + (mi_row + r) * stride + mi_col;
  memset(row_ptr, 0, mi_wide * sizeof(*row_ptr));
}

// Standard copy algorithm in C++
for (int j = 0; j < bh + BORDER; ++j) {
  std::copy(inter_pred + j * stride, inter_pred + j * stride + bw + BORDER,
            inter_input + j * (bw + BORDER));
}
```
