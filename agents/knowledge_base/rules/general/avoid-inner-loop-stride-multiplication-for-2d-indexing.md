---
name: libaom/avoid-inner-loop-stride-multiplication-for-2d-indexing
description: When traversing 2D image buffers, blocks, or DSP kernels, advance pointers incrementally by stride or use memcpy instead of recalculating row index multiplied by stride.
rubric: safety
severity: should_fix
---

**Rule.** When iterating over 2D image frames, blocks, or buffers in nested
loops (including SIMD and DSP kernels), do not recompute 2D element or block
offsets using stride multiplication (such as `row * stride + col`, `i * step + j
* step * stride`, or `col * stride`) inside loop bodies. Instead, maintain
running row pointers and advance them incrementally across outer loop iterations
(e.g., `src_row += stride` or `src += stride`), use row-wise `memcpy` for
contiguous row copying, or update inner loop pointers directly (e.g.,
`*dst_ptr++` or `dst_ptr += stride`) for transposed or block traversals.

**Why.** Recalculating 2D stride offsets (`row * stride` or `col * stride`)
repeatedly inside nested loop bodies generates redundant multiplication and
base-index addition instructions for every element access, increasing register
pressure and hindering compiler auto-vectorization. Advancing pointers directly
via stride addition or using `memcpy` allows compilers and hardware to utilize
efficient post-increment and base-offset addressing modes (e.g., on x86 and
ARM), significantly reducing arithmetic overhead in performance-critical DSP and
block processing loops.

## Examples

Bad:

```cpp
// Bad: Redundant stride multiplications in 2D block and transposed loops
for (int r = 0; r < height; ++r) {
  for (int c = 0; c < width; ++c) {
    dst[r * dst_stride + c] = src[r * src_stride + c];
    transposed[c * 128 + (height - 1 - r)] = src[r * src_stride + c];
  }
}

// Bad: Recalculating 2D stride offsets in SIMD block processing loops
for (int j = 0; j < h / 64; ++j) {
  for (int i = 0; i < w / 64; ++i) {
    const uint8_t *src_ptr = src + i * 64 + j * 64 * src_stride;
    const uint8_t *ref_ptr = ref + i * 64 + j * 64 * ref_stride;
    process_block_64x64(src_ptr, src_stride, ref_ptr, ref_stride);
  }
}
```

Good:

```cpp
// Good: Advance row pointers incrementally in outer loop and step inner pointers
const uint8_t *src_row = src;
uint8_t *dst_row = dst;
for (int r = 0; r < height; ++r) {
  uint8_t *trans_ptr = transposed + (height - 1 - r);
  for (int c = 0; c < width; ++c) {
    dst_row[c] = src_row[c];
    *trans_ptr = src_row[c];
    trans_ptr += 128;
  }
  src_row += src_stride;
  dst_row += dst_stride;
}

// Good: Advance SIMD block and row pointers across loop iterations
const uint8_t *src_row = src;
const uint8_t *ref_row = ref;
for (int j = 0; j < h / 64; ++j) {
  const uint8_t *src_ptr = src_row;
  const uint8_t *ref_ptr = ref_row;
  for (int i = 0; i < w / 64; ++i) {
    process_block_64x64(src_ptr, src_stride, ref_ptr, ref_stride);
    src_ptr += 64;
    ref_ptr += 64;
  }
  src_row += 64 * src_stride;
  ref_row += 64 * ref_stride;
}
```
