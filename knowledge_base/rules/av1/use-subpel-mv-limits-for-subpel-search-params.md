---
name: libaom/av1/use-subpel-mv-limits-for-subpel-search-params
description: When referencing motion vector limits in motion search parameter structures, use SubpelMvLimits for SUBPEL_MOTION_SEARCH_PARAMS and FullMvLimits for FULLPEL_MOTION_SEARCH_PARAMS
rubric: correctness
severity: should_fix
---

**Rule.** When referencing, aliasing, or passing pointers to motion vector
search limits (mv_limits), ensure the pointer type matches the coordinate
precision of the motion search parameter structure: use SubpelMvLimits for
SUBPEL_MOTION_SEARCH_PARAMS (1/8-pel coordinates) and FullMvLimits for
FULLPEL_MOTION_SEARCH_PARAMS (integer full-pixel coordinates). Do not alias
SUBPEL_MOTION_SEARCH_PARAMS.mv_limits with FullMvLimits * or vice versa.

**Why.** SubpelMvLimits and FullMvLimits share identical struct layouts
(col_min, col_max, row_min, row_max) but represent motion vector bounds in
different coordinate precisions (1/8th-subpixel vs full-pixel units). Using
mismatched pointer types creates type confusion, risks passing subpel limits to
fullpel clamping/range checks, and can lead to out-of-bounds motion vector
searches or compiler diagnostics.

## Examples

Bad:

```cpp
// In subpel search routines with SUBPEL_MOTION_SEARCH_PARAMS *ms_params:
FullMvLimits *mv_limits = &ms_params->mv_limits;

// In fullpel search routines with FULLPEL_MOTION_SEARCH_PARAMS *ms_params:
SubpelMvLimits *mv_limits = &ms_params->mv_limits;
```

Good:

```cpp
// In subpel search routines with SUBPEL_MOTION_SEARCH_PARAMS *ms_params:
SubpelMvLimits *mv_limits = &ms_params->mv_limits;

// In fullpel search routines with FULLPEL_MOTION_SEARCH_PARAMS *ms_params:
FullMvLimits *mv_limits = &ms_params->mv_limits;
```
