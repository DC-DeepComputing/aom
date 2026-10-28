---
name: libaom/av1/complete-filter-and-convolution-function-families
description: When adding masked variants to convolution or restoration filter families, implement corresponding functions across all input modes (single and dual-input) and bitdepth tiers (standard and highbd)
rubric: correctness
severity: should_fix
---

**Rule.** When extending a family of convolution, restoration, or DSP filtering
functions with new capabilities (such as mask support or specialized blending),
implement the corresponding variants across all input configurations
(single-input and dual-input / cross-component) and bitdepth tiers (standard
8-bit and highbd). Do not leave dual-input or high-bitdepth counterparts omitted
when introducing new filtering variants.

**Why.** libaom structures convolution and restoration filtering routines into
coherent function families covering both single-channel and dual-input pipelines
across standard and high bitdepths. Omitting masked or specialized variants for
dual-input or highbd paths leaves the function family incomplete, leading to
missing functionality or runtime dispatch failures when cross-component or
high-bitdepth filtering requires those features.

## Examples

Bad:

```cpp
// Incomplete family: Masked variants added only for single-input convolution
void av1_convolve_2d_mask(const uint8_t *src, int src_stride, ...);
void av1_highbd_convolve_2d_mask(const uint16_t *src, int src_stride, ...);

// Dual-input / cross-component functions lack masked counterparts:
void av1_convolve_2d_dual(const uint8_t *src0, const uint8_t *src1, ...);
void av1_highbd_convolve_2d_dual(const uint16_t *src0, const uint16_t *src1, ...);
```

Good:

```cpp
// Complete family: Both single-input and dual-input have standard and highbd masked variants
void av1_convolve_2d_mask(const uint8_t *src, int src_stride, ...);
void av1_highbd_convolve_2d_mask(const uint16_t *src, int src_stride, ...);

void av1_convolve_2d_dual_mask(const uint8_t *src0, const uint8_t *src1, ...);
void av1_highbd_convolve_2d_dual_mask(const uint16_t *src0, const uint16_t *src1, ...);
```
