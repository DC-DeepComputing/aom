---
name: libaom/av1/unified-2d-convolution-dispatch-over-1d-branching
description: When implementing C fallback convolution dispatch for sub-pixel filtering with identity phase-0 tables, use unified 2D convolution rather than branching into 1D routines
rubric: design
severity: should_fix
---

**Rule.** In AV1 C fallback convolution dispatch paths, use unified 2D
convolution functions (such as av1_convolve_2d_sr_c or
av1_highbd_convolve_2d_sr_c) with complete filter phase tables instead of
branching into separate 1D convolution routines for single-axis sub-pixel
shifts. Ensure filter tables define identity filter entries for phase 0 so that
2D convolution handles 1D sub-pixel filtering directly.

**Why.** AV1 2D convolution routines natively support both 1D and 2D sub-pixel
filtering when filter coefficient tables provide identity filter entries for
phase 0 (integer pel). Relying on full filter tables simplifies dispatch logic,
eliminates redundant conditional branches, and avoids code duplication across 1D
and 2D paths in C fallback code.

## Examples

Bad:

```cpp
if (subpel_x_qn && subpel_y_qn) {
  av1_convolve_2d_sr_c(src, src_stride, dst, dst_stride, w, h,
                       filter_params_x, filter_params_y, subpel_x_qn,
                       subpel_y_qn, conv_params);
} else if (subpel_x_qn) {
  av1_convolve_x_sr_c(src, src_stride, dst, dst_stride, w, h,
                      filter_params_x, filter_params_y, subpel_x_qn,
                      0, conv_params);
} else {
  av1_convolve_y_sr_c(src, src_stride, dst, dst_stride, w, h,
                      filter_params_x, filter_params_y, 0,
                      subpel_y_qn, conv_params);
}
```

Good:

```cpp
av1_convolve_2d_sr_c(src, src_stride, dst, dst_stride, w, h,
                     filter_params_x, filter_params_y, subpel_x_qn,
                     subpel_y_qn, conv_params);
```
