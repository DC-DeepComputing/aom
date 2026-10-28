---
name: libaom/av1/pass-filter-pointers-directly-in-simd-convolve
description: In SIMD convolution routines, pass and load filter coefficient pointers directly instead of recovering table bases with legacy get_filter_base and get_filter_offset pointer masking
rubric: readability
severity: should_fix
---

**Rule.** In SIMD convolution implementations (such as Wiener convolution or ARM
NEON/x86 convolve kernels), pass and load filter coefficient pointers directly
rather than using legacy get_filter_base() and get_filter_offset() helper
functions that rely on filter table alignment pointer masking.

**Why.** Wiener convolution and direct SIMD filter kernels pass filter
coefficient arrays directly rather than indexing through an aligned subpel
interpolation table. Using get_filter_base() and get_filter_offset() introduces
redundant pointer and offset arithmetic and relies on fragile 256-byte alignment
assumptions about filter coefficient tables.

## Examples

Bad:

```cpp
static const InterpKernel *get_filter_base(const int16_t *filter) {
  return (const InterpKernel *)(((intptr_t)filter) & ~((intptr_t)0xFF));
}
static int get_filter_offset(const int16_t *f, const InterpKernel *base) {
  return (int)((const InterpKernel *)(intptr_t)f - base);
}

void av1_highbd_wiener_convolve_add_src_neon(...) {
  const InterpKernel *const x_filters_ptr = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, x_filters_ptr);
  // ...
}
```

Good:

```cpp
void av1_highbd_wiener_convolve_add_src_neon(
    const uint8_t *src8, ptrdiff_t src_stride, uint8_t *dst8,
    ptrdiff_t dst_stride, const int16_t *x_filter_ptr, int x_step_q4,
    const int16_t *y_filter_ptr, int y_step_q4, int w, int h,
    const ConvolveParams *conv_params, int bd) {
  highbd_convolve_add_src_horiz_hip(src_ptr, src_stride, im_block, im_stride,
                                    x_filter_ptr, x_step_q4, w, im_h,
                                    conv_params->round_0, bd);
}
```
