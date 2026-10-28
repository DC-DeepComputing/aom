---
name: libaom/qualify-subregion-dimension-names
description: When naming variables or parameters for subregions, partial blocks, or planes, use qualifying prefixes like part_width or blk_width rather than bare width or height
rubric: readability
severity: nit
---

**Rule.** When declaring variables or parameters that represent the dimensions
of subregions, partial blocks, tiles, or subsampled planes, use descriptive
qualifying prefixes (such as `part_width`/`part_height`,
`blk_width`/`blk_height`, `sub_width`/`sub_height`, or
`plane_width`/`plane_height`) rather than generic `width`/`height`. Ensure the
name clearly distinguishes partial or local dimensions from full frame, crop, or
buffer dimensions (`frame_width`, `y_crop_width`).

**Why.** In image and video codec pipelines, using unqualified `width` and
`height` identifiers when operating on subregions or chroma planes easily leads
to confusion with full frame or buffer dimensions, increasing the risk of
dimension mismatches and out-of-bounds indexing bugs across caller/callee
boundaries.

## Examples

Bad:

```cpp
// Ambiguous dimension names when calculating subregion or subsampled dimensions
int uv_width = width >> subsampling_x;
int uv_height = height >> subsampling_y;
process_subregion(src, uv_width, uv_height, stride);
```

Good:

```cpp
// Explicitly qualified variable names distinguishing subregion dimensions from full frame dimensions
int part_width = width >> subsampling_x;
int part_height = height >> subsampling_y;
process_subregion(src, part_width, part_height, stride);
```
