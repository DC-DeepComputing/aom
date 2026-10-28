---
name: libaom/av1/include-block-dimensions-when-clamping-search-bounds
description: When clamping motion search bounds or window offsets against frame buffer borders, include block dimensions (bw and bh) in right and bottom boundary checks
rubric: safety
severity: blocking
---

**Rule.** When calculating right or bottom motion estimation search bounds or
candidate window offsets against padded frame buffer dimensions (such as
cm->width + border or cm->height + border), include the block dimensions (bw or
bh) in both the boundary condition checks and clamping calculations. Ensure
right and bottom limits satisfy ((pos_col or (mi_col << 2)) +
search_size_width_right + bw <= cm->width + border) and ((pos_row or (mi_row <<
2)) + search_size_height_bottom + bh <= cm->height + border), subtracting bw or
bh when clamping, rather than checking top-left coordinates alone.

**Why.** Evaluating or projecting candidate blocks of size bw x bh starting at
search offset (x, y) accesses pixels up to x + search_offset + bw and y +
search_offset + bh. Clamping right and bottom search bounds using only top-left
block origin coordinates without including block width and height allows
candidate reads at maximum positive displacements to extend past allocated frame
padding, leading to out-of-bounds buffer reads and memory safety violations.

## Examples

Bad:

```cpp
// Bad: Clamping right/bottom bounds against top-left origin ignores block width/height
if (((mi_col << 2) + search_size_width_right) > cm->width + border) {
  search_size_width_right = cm->width + border - (mi_col << 2);
}
if (((mi_row << 2) + search_size_height_bottom) > cm->height + border) {
  search_size_height_bottom = cm->height + border - (mi_row << 2);
}
```

Good:

```cpp
// Good: Include block dimensions (bw, bh) in boundary checks and clamping math
if (((mi_col << 2) + search_size_width_right + bw) > cm->width + border) {
  search_size_width_right = cm->width + border - (mi_col << 2) - bw;
}
if (((mi_row << 2) + search_size_height_bottom + bh) > cm->height + border) {
  search_size_height_bottom = cm->height + border - (mi_row << 2) - bh;
}
```
