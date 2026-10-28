---
name: libaom/aom_dsp/document-buffer-layouts-and-stride-semantics
description: When declaring functions with non-standard buffer layouts or multi-block offsets, document memory organization and use explicit parameter names like src_blk_stride instead of sstride
rubric: readability
severity: should_fix
---

**Rule.** When defining or declaring DSP kernels, SIMD optimizations, RTCD
primitives, or helper functions that operate on non-standard or asymmetric
memory layouts (such as sequentially packed sub-blocks, multi-block batches like
dual/quad, or mixed planar/interleaved buffers alongside standard 2D frame
buffers):
1. Precede the function declaration with a doc comment describing the memory organization, sub-block packing order, and stride semantics for each buffer parameter (e.g., sequentially stored sub-blocks within filter blocks vs. 2D raster frame buffers).
2. Distinguish block-level stride parameters from conventional row pitch by using explicit parameter names (such as src_blk_stride, dst_blk_stride, blk_stride, or block_stride) rather than generic stride, sstride, or dstride.

**Why.** In video codecs and DSP routines, stride, sstride, and dstride
conventionally denote 2D line/row pitch (the distance in bytes or pixels between
consecutive vertical rows). When functions operate on multi-block batches or
sequentially packed sub-blocks, using standard row stride identifiers obscures
buffer layout assumptions and misleads callers. Explicitly naming sub-block
strides and documenting memory layouts prevents buffer over-reads, incorrect
stride calculations, and integration bugs across C reference code and SIMD
backends.

## Examples

Bad:

```cpp
// Compute MSE of four consecutive 4x4 blocks.
// Misleading: sstride represents block-to-block offset, and layout is undocumented.
uint64_t aom_mse_4xh_quad_16bit_avx2(uint8_t *dst, int dstride,
                                     const uint16_t *src, int sstride, int h);

uint64_t aom_mse_8xh_dual_16bit_avx2(uint8_t *dst, int dstride,
                                     const uint16_t *src, int sstride, int h);
```

Good:

```cpp
// Compute MSE of four consecutive 4x4 blocks.
// In src buffer, each 4x4 block in a 32x32 filter block is stored sequentially.
// Hence src_blk_stride is the block width. dst buffer is a frame buffer, thus
// dstride is a frame-level row stride.
uint64_t aom_mse_4xh_quad_16bit_avx2(uint8_t *dst, int dstride,
                                     const uint16_t *src,
                                     int src_blk_stride, int h);

// In src, 8x8 sub-blocks are stored sequentially (src_blk_stride equals block
// width), whereas dst is a frame buffer with row pitch dstride.
uint64_t aom_mse_8xh_dual_16bit_avx2(uint8_t *dst, int dstride,
                                     const uint16_t *src,
                                     int src_blk_stride, int h);
```
