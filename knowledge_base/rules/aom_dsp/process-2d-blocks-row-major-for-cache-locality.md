---
name: libaom/aom_dsp/process-2d-blocks-row-major-for-cache-locality
description: When implementing 2D image or SIMD block-processing kernels, iterate subblocks in row-major order or process full rows sequentially rather than column-major to maximize cache locality
rubric: safety
severity: should_fix
---

**Rule.** When implementing 2D image or SIMD block-processing routines (such as
SAD, SSE, variance, or distortion kernels), iterate across subblocks in
row-major order or process full rows across the block width sequentially. Do not
iterate subblocks in column-major order or traverse down vertical block columns
before visiting adjacent horizontal blocks.

**Why.** Image and frame buffers in libaom are stored in row-major layout with
stride offsets. Accessing subblocks in column-major order repeatedly traverses
down multiple cache lines and reloads earlier rows, causing cache evictions.
Processing sequentially across entire rows or in row-major order preserves cache
locality and improves throughput.

## Examples

Bad:

```cpp
// Anti-pattern: Column-major block iteration jumps down rows before visiting right blocks.
unsigned int aom_sad128x128_neon(const uint8_t *src, int src_stride,
                                 const uint8_t *ref, int ref_stride) {
  unsigned int sum = 0;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      const uint8_t *src_64 = src + i * 64 + j * 64 * src_stride;
      const uint8_t *ref_64 = ref + i * 64 + j * 64 * ref_stride;
      sum += aom_sad64x64_neon(src_64, src_stride, ref_64, ref_stride);
    }
  }
  return sum;
}
```

Good:

```cpp
// Preferred: Process full row width sequentially before stepping to the next row.
unsigned int aom_sad128x128_neon(const uint8_t *src, int src_stride,
                                 const uint8_t *ref, int ref_stride) {
  uint32x4_t vec_accum_32lo = vdupq_n_u32(0);
  uint32x4_t vec_accum_32hi = vdupq_n_u32(0);
  for (int i = 0; i < 128; ++i) {
    const uint8x16_t vec_src_0 = vld1q_u8(src);
    const uint8x16_t vec_src_16 = vld1q_u8(src + 16);
    // ... load remaining chunks across full 128-byte row ...
    src += src_stride;
    ref += ref_stride;
  }
  return vaddvq_u32(vec_accum_32lo) + vaddvq_u32(vec_accum_32hi);
}
```
