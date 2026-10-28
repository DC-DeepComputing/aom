---
name: libaom/av1/allow-padded-filter-taps-in-8tap-simd-convolve
description: When dispatching AV1 SIMD convolve kernels with 8-tap implementations, check filter_taps > 8 instead of != 8 to allow zero-padded 4- and 6-tap filters
rubric: safety
severity: should_fix
---

**Rule.** In AV1 SIMD convolve implementations dispatching to 8-tap convolution
routines, check filter_taps > 8 rather than filter_taps != 8 when falling back
to C reference code. Allow 4-tap and 6-tap interpolation filters to execute via
the 8-tap SIMD path.

**Why.** In AV1 interpolation filter kernel tables, smaller 4-tap and 6-tap
filters are zero-padded to 8 taps. An 8-tap SIMD convolution kernel correctly
computes the result when outer taps are zero, so checking filter_taps > 8
enables SIMD acceleration for shorter filters instead of falling back to slower
C code.

## Examples

Bad:

```cpp
const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
if (y_filter_taps != 8) {
  av1_highbd_convolve_y_sr_c(src, src_stride, dst, dst_stride, w, h,
                             filter_params_y, subpel_y_qn, bd);
  return;
}
```

Good:

```cpp
const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
if (y_filter_taps > 8) {
  av1_highbd_convolve_y_sr_c(src, src_stride, dst, dst_stride, w, h,
                             filter_params_y, subpel_y_qn, bd);
  return;
}
```
