---
name: libaom/encapsulate-index-and-coordinate-calculation-in-helpers
description: When computing filter kernel indices, reference coordinates, or subpixel offsets, encapsulate bounds wrapping, coordinate adjustments, and scaling inside dedicated helper functions rather than caller sites
rubric: design
severity: should_fix
---

**Rule.** Encapsulate complete index calculations, bounds checking, reference
coordinate adjustments (such as ceil or floor), and scale factors inside
dedicated helper functions. Do not split coordinate and index calculations
across caller sites or perform partial index math and post-adjustment before
passing indices to sampling or interpolation routines.

**Why.** Splitting coordinate and index calculations across call sites
duplicates non-trivial bounds and scaling logic, clutters caller functions, and
increases the risk of omission or inconsistent index calculations across
different call sites.

## Examples

Bad:

```cpp
x_kernel_idx = cal_kernel_idx(subpel_x);
if (x_kernel_idx > filter_tap - 1) {
  x_kernel_idx = x_kernel_idx % filter_tap;
  x_ref = (int)ceil(subpel_x);
} else {
  x_ref = (int)floor(subpel_x);
}
get_interp_x_arr(..., x_kernel_idx * 2, ...);
```

Good:

```cpp
static int cal_kernel_idx(double subpel_pos, int *pos_ref) {
  int idx = ...;
  if (idx > filter_tap - 1) {
    idx %= filter_tap;
    *pos_ref = (int)ceil(subpel_pos);
  } else {
    *pos_ref = (int)floor(subpel_pos);
  }
  return idx * 2;
}

x_kernel_idx = cal_kernel_idx(subpel_x, &x_ref);
get_interp_x_arr(..., x_kernel_idx, ...);
```
