---
name: libaom/av1/use-predefined-zero-mv-constants
description: When initializing, resetting, or passing zero motion vectors, use predefined constants such as kZeroMv or kZeroFullMv from av1/common/mv.h instead of ad-hoc local zero structs
rubric: readability
severity: nit
---

**Rule.** When initializing, resetting, or passing zero motion vectors, use the
predefined `kZeroMv` (for `MV`) or `kZeroFullMv` (for `FULLPEL_MV`) constants
from `av1/common/mv.h`. Do not declare ad-hoc local zero-initialized `MV` or
`FULLPEL_MV` structs or pass temporary `{ 0, 0 }` dummy variables.

**Why.** Using standard predefined motion vector constants eliminates
unnecessary stack variable allocations and boilerplate across encoder and
decoder routines, ensuring consistent motion vector handling throughout the AV1
codebase.

## Examples

Bad:

```cpp
// Creating an ad-hoc local zero motion vector struct
const MV dummy_mv = { 0, 0 };
*y_sad = av1_int_pro_motion_estimation(
    cpi, x, cm->seq_params->sb_size, mi_row, mi_col, &dummy_mv,
    &y_sad_zero, me_search_par);
```

Good:

```cpp
// Using predefined constant from av1/common/mv.h
*y_sad = av1_int_pro_motion_estimation(
    cpi, x, cm->seq_params->sb_size, mi_row, mi_col, &kZeroMv,
    &y_sad_zero, me_search_par);
```
