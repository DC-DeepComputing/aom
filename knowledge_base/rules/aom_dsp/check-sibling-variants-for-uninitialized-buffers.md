---
name: libaom/aom_dsp/check-sibling-variants-for-uninitialized-buffers
description: When fixing uninitialized buffer or SIMD vector warnings in DSP or transform routines, update all sibling variants in the family that share the partial-initialization pattern
rubric: correctness
severity: should_fix
---

**Rule.** When fixing uninitialized local buffer or SIMD vector warnings in a
DSP or transform routine (such as when a buffer is only partially loaded with
coefficients), inspect and update all sibling functions in the same family that
share the partial-initialization pattern. Ensure unpopulated vector or scalar
elements are explicitly zeroed before subsequent processing across all related
size or coefficient variants.

**Why.** Specialized transform and DSP routines (e.g. partial-coefficient IDCT
or DCT variants) often share identical stack buffer allocation and
partial-loading logic across multiple block sizes and coefficient limits. Fixing
an uninitialized data hazard or compiler warning in only one variant leaves
latent uninitialized memory reads or compiler warnings in sibling functions.

## Examples

Bad:

```cpp
// Only fixing one variant leaves sibling partial-coefficient transform uninitialized
void aom_idct32x32_135_add_avx2(const tran_low_t *input, uint8_t *dest,
                                int stride) {
  __m256i in[32];
  // in[16..31] remain uninitialized
  load_buffer_from_32x32(input, in, 16);
  idct32x32_135_kernel_avx2(in, dest, stride);
}
```

Good:

```cpp
// All sibling variants in the family explicitly zero the buffer before partial loads
void aom_idct32x32_135_add_avx2(const tran_low_t *input, uint8_t *dest,
                                int stride) {
  __m256i in[32];
  zero_buffer(in, 32);
  load_buffer_from_32x32(input, in, 16);
  idct32x32_135_kernel_avx2(in, dest, stride);
}
```
