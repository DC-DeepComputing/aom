---
name: libaom/use-dimension-suffixes-not-coordinate-suffixes-for-sizes
description: When naming variables representing dimensions, spans, or extents, use width and height suffixes (_w, _h) rather than coordinate identifiers (_x, _y)
rubric: readability
severity: nit
---

**Rule.** Name variables, struct members, and function parameters representing
dimensions, extents, spans, or block sizes using width and height suffixes (such
as `_w`, `_h`, `width`, or `height`) rather than coordinate identifiers (such as
`_x` or `_y`). Reserve `_x` and `_y` (or `col` and `row`) strictly for spatial
positions, coordinates, and offsets.

**Why.** Coordinate suffixes (`_x`, `_y`) denote spatial positions or offsets
within a frame, whereas dimension suffixes (`_w`, `_h`) denote extents, widths,
or heights. Conflating coordinates with dimensions misleads readers about
whether a variable represents a point or a span, increasing the risk of
off-by-one and boundary errors.

## Examples

Bad:

```cpp
// Using coordinate suffixes (_x, _y) for working dimensions
const int work_x = AOMMIN(16, dst_w - x);
const int work_y = AOMMIN(16, dst_h - y);
if (work_x != 16 || work_y != 16) {
  process_block(src, dst, work_x, work_y);
}
```

Good:

```cpp
// Using dimension suffixes (_w, _h) for working dimensions
const int work_w = AOMMIN(16, dst_w - x);
const int work_h = AOMMIN(16, dst_h - y);
if (work_w != 16 || work_h != 16) {
  process_block(src, dst, work_w, work_h);
}
```
