---
name: libaom/av1/use-bit-shifts-over-get-mv-rawpel-for-aligned-mv-bounds
description: When converting full-pixel-aligned motion vector bounds or search limits from 1/8-pel units, use explicit bit-shifts (>> 3) instead of GET_MV_RAWPEL
rubric: readability
severity: nit
---

**Rule.** When converting motion vector bounds, search limits, or
fullpel-aligned constants (such as MV_LOW and MV_UPP) from 1/8-pel units to
full-pixel coordinates, use explicit bit-shifts (>> 3) rather than the
round-to-nearest macro GET_MV_RAWPEL. Reserve GET_MV_RAWPEL strictly for
rounding unaligned subpixel motion vectors to full-pixel coordinates.

**Why.** GET_MV_RAWPEL adds an offset before shifting to round subpel
coordinates to the nearest fullpel. Applying it to exact or aligned boundary
constants falsely implies round-to-nearest behavior, whereas explicit bit-shifts
correctly and unambiguously express directional coordinate truncation.

## Examples

Bad:

```cpp
// Using round-to-nearest macro for fullpel-aligned boundary constants
col_min = AOMMAX(col_min, GET_MV_RAWPEL(MV_LOW) + 1);
row_min = AOMMAX(row_min, GET_MV_RAWPEL(MV_LOW) + 1);
col_max = AOMMIN(col_max, GET_MV_RAWPEL(MV_UPP) - 1);
row_max = AOMMIN(row_max, GET_MV_RAWPEL(MV_UPP) - 1);
```

Good:

```cpp
// Using explicit bit-shifts for fullpel-aligned boundary constants
col_min = AOMMAX(col_min, (MV_LOW >> 3) + 1);
row_min = AOMMAX(row_min, (MV_LOW >> 3) + 1);
col_max = AOMMIN(col_max, (MV_UPP >> 3) - 1);
row_max = AOMMIN(row_max, (MV_UPP >> 3) - 1);
```
