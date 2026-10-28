---
name: libaom/av1/use-interp-filter-params-table-for-kernels
description: When retrieving interpolation filter kernels or parameters for an InterpFilter, access av1_interp_filter_params_list or filter.h helpers instead of open-coding switch statements
rubric: readability
severity: should_fix
---

**Rule.** When retrieving interpolation filter kernels or parameters for an
InterpFilter enum value, access the centralized av1_interp_filter_params_list
lookup table (or helper functions in av1/common/filter.h, such as
av1_get_interp_filter_params_with_block_size or av1_get_interp_filter_kernel).
Do not open-code redundant switch or if-else blocks mapping filter enum values
to individual filter arrays (such as av1_bilinear_filters or
av1_sub_pel_filters_8*).

**Why.** av1_interp_filter_params_list centrally maps InterpFilter enum values
to their corresponding InterpFilterParams and filter kernel pointers
(filter_ptr). Reusing the centralized table or its accessor helpers eliminates
duplicated mapping logic, avoids maintenance divergence across resizing and
filtering routines, and keeps kernel lookup consistent.

## Examples

Bad:

```cpp
const InterpKernel *kernel = NULL;
switch (filter) {
  case BILINEAR: kernel = av1_bilinear_filters; break;
  case EIGHTTAP_SMOOTH: kernel = av1_sub_pel_filters_8smooth; break;
  case EIGHTTAP_REGULAR: kernel = av1_sub_pel_filters_8; break;
  default: assert(false);
}
```

Good:

```cpp
assert(filter == BILINEAR || filter == EIGHTTAP_SMOOTH ||
       filter == EIGHTTAP_REGULAR);
const InterpKernel *const kernel =
    (const InterpKernel *)av1_interp_filter_params_list[filter].filter_ptr;
```
