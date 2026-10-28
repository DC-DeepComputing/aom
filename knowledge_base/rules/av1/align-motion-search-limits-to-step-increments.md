---
name: libaom/av1/align-motion-search-limits-to-step-increments
description: When configuring search ranges or boundary fallback limits for stepped or coarse-to-fine motion estimation routines, align boundary limits to coarse step increments
rubric: correctness
severity: should_fix
---

**Rule.** When configuring search ranges or boundary fallback limits for
stepped, coarse-to-fine, or 1D projection motion estimation routines (such as
av1_vector_match), ensure boundary and search limits are aligned to multiples of
the coarse search step size (e.g., truncating to multiples of 16 pixels via
(border >> 4) << 4). Do not pass unaligned raw border or search bounds to coarse
search stages.

**Why.** Coarse search stages in motion estimation evaluate candidate positions
in fixed stride increments (such as 16-pixel steps). Using unaligned boundary or
search limits can cause search windows to misalign with candidate grids or
exceed valid reference boundaries.

## Examples

Bad:

```cpp
// Unaligned border limit passed to coarse 1D projection motion search
const int border = cpi->oxcf.border_in_pixels;
if ((pos_col - search_size_width < -border) ||
    (pos_col + search_size_width > cm->width + border)) {
  search_size_width = border;
}
*best_intmv_col = av1_vector_match(hbuf, src_hbuf, bwl, search_size_width, ...);
```

Good:

```cpp
// Truncate border to a multiple of 16 to align with coarse search step size
const int border = (cpi->oxcf.border_in_pixels >> 4) << 4;
if ((pos_col - search_size_width < -border) ||
    (pos_col + search_size_width > cm->width + border)) {
  search_size_width = border;
}
*best_intmv_col = av1_vector_match(hbuf, src_hbuf, bwl, search_size_width, ...);
```
