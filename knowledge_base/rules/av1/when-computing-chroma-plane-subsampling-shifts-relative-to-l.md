---
name: libaom/av1/when-computing-chroma-plane-subsampling-shifts-relative-to-l
description: When computing chroma plane subsampling shifts relative to luma in AV1, read plane subsampling values directly instead of subtracting luma plane subsampling
rubric: readability
severity: nit
---

**Rule.** In AV1 plane handling, simplify plane subsampling shift calculations
by reading mbd->plane[plane].subsampling_x and subsampling_y directly. Do not
redundantly subtract luma plane subsampling values
(mbd->plane[AOM_PLANE_Y].subsampling_x or mbd->plane[0]), as the luma plane is
never subsampled.

**Why.** In AV1, the luma (Y) plane is always full resolution with subsampling
factors equal to 0. Subtracting luma plane subsampling values is a redundant
identity subtraction that adds unnecessary arithmetic and visual clutter.

## Examples

Bad:

```cpp
const int ss_x_shift =
    mbd->plane[plane].subsampling_x - mbd->plane[AOM_PLANE_Y].subsampling_x;
const int ss_y_shift =
    mbd->plane[plane].subsampling_y - mbd->plane[AOM_PLANE_Y].subsampling_y;
```

Good:

```cpp
const int ss_x_shift = mbd->plane[plane].subsampling_x;
const int ss_y_shift = mbd->plane[plane].subsampling_y;
```
