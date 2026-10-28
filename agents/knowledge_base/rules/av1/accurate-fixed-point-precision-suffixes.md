---
name: libaom/av1/accurate-fixed-point-precision-suffixes
description: When naming fixed-point coordinate, step, and offset parameters in convolution or filtering functions, use precision suffixes matching their representation (e.g., _qn vs. _q4).
rubric: readability
severity: should_fix
---

**Rule.** When naming fixed-point coordinate, offset, and step parameters in
convolution, interpolation, and scaling functions, use precision suffixes that
accurately reflect their fractional bit-depth (such as _qn for variable or N-bit
scaled subpixel precision SCALE_SUBPEL_BITS, and _q4 for 4-bit 1/16th-pel
precision SUBPEL_BITS). Ensure parameter names with these precision suffixes are
consistent across declarations, definitions, and RTCD prototypes.

**Why.** Fixed-point coordinate and step parameters must convey their exact
expected fractional precision to callers and implementers. Using an inaccurate
suffix (such as _q4 for a parameter expecting 10-bit scaled precision _qn) leads
to coordinate format mismatches, improper shift amounts, and out-of-bounds
indexing bugs.

## Examples

Bad:

```cpp
// Bad: subpel_x_q4 and subpel_y_q4 incorrectly suggest 4-bit 1/16-pel precision
// for scaled convolution parameters that require 10-bit SCALE_SUBPEL_BITS precision.
void av1_convolve_2d_scale_c(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *filter_params_x,
                             const InterpFilterParams *filter_params_y,
                             const int subpel_x_q4, const int x_step_qn,
                             const int subpel_y_q4, const int y_step_qn,
                             ConvolveParams *conv_params);
```

Good:

```cpp
// Good: subpel_x_qn and subpel_y_qn accurately reflect N-bit scaled precision
// and match step parameters across declarations and definitions.
void av1_convolve_2d_scale_c(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *filter_params_x,
                             const InterpFilterParams *filter_params_y,
                             const int subpel_x_qn, const int x_step_qn,
                             const int subpel_y_qn, const int y_step_qn,
                             ConvolveParams *conv_params);
```
