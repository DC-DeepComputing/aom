---
name: libaom/av1/distinguish-filter-taps-from-subpel-shifts
description: When bounding, indexing, or wrapping subpixel interpolation filter phases, use subpel shift constants (SUBPEL_SHIFTS) rather than filter tap counts (SUBPEL_TAPS)
rubric: correctness
severity: should_fix
---

**Rule.** When bounding, wrapping, or indexing subpixel filter phases (such as
calculating kernel offsets or phase indices for subpixel interpolation), use the
dedicated subpixel phase count or shift constants (SUBPEL_SHIFTS,
BIL_SUBPEL_SHIFTS, SCALE_SUBPEL_SHIFTS, or SUBPEL_MASK). Do not use filter tap
lengths or kernel sizes (such as filter_tap, num_taps, or SUBPEL_TAPS) to bound
or modulo subpixel phase indices.

**Why.** Filter tap count (the number of filter coefficients per phase, such as
2, 4, 8, or 12 taps) and subpixel phase count (the number of fractional subpixel
positions, such as 8 or 16 shifts) represent two distinct dimensions of
interpolation filter tables. Using filter tap length to bound or modulo phase
indices causes subtle logic bugs and breaks interpolation when filters with
different tap sizes are selected.

## Examples

Bad:

```cpp
// WRONG: Using filter tap count to modulo or bound the subpixel filter phase index
if (x_kernel_idx > filter_tap - 1) {
  x_kernel_idx = x_kernel_idx % filter_tap;
  x_ref = (int)ceil(subpel_x);
}
```

Good:

```cpp
// CORRECT: Using dedicated subpixel shift constants for phase indexing
if (x_kernel_idx >= SUBPEL_SHIFTS) {
  x_kernel_idx = x_kernel_idx % SUBPEL_SHIFTS;
  x_ref = (int)ceil(subpel_x);
}
```
