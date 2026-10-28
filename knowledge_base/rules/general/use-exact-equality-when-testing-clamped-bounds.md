---
name: libaom/use-exact-equality-when-testing-clamped-bounds
description: When branching on a variable already clamped to a minimum or maximum bound, test for exact equality against the bound rather than inequality
rubric: readability
severity: nit
---

**Rule.** When branching on or validating a value that has already been clamped
or bounded to an extreme (such as via ternary clamping, AOMMAX, AOMMIN, or
clamp), check for exact equality (val == BOUND) rather than an inequality (val
<= MIN or val >= MAX). Do not use relational inequality operators against bounds
when values beyond the threshold are impossible by construction.

**Why.** Testing an inequality against a lower or upper bound when values beyond
that bound are unreachable by construction obscures the underlying invariant and
misleads readers into thinking out-of-range values are possible.

## Examples

Bad:

```cpp
// Clamped to minimum bound of 6
const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;
if (clamped_y_taps <= 6) {
  dist_wtd_convolve_2d_vert_6tap_neon(src, src_stride, dst, dst_stride, w, h,
                                      y_filter, conv_params);
}
```

Good:

```cpp
// Clamped to minimum bound of 6
const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;
if (clamped_y_taps == 6) {
  dist_wtd_convolve_2d_vert_6tap_neon(src, src_stride, dst, dst_stride, w, h,
                                      y_filter, conv_params);
}
```
