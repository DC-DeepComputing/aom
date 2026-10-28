---
name: libaom/av1/validate-directional-tile-dimensions-independently
description: When validating directional tile dimension constraints like minimum tile width or height, check only the relevant dimension count without coupling orthogonal dimensions
rubric: correctness
severity: should_fix
---

**Rule.** When validating directional tile dimension constraints (such as
minimum tile width or height limits that exempt the rightmost column or bottom
row), evaluate or bypass the check based solely on the count of the dimension
being validated (e.g., `tile_cols == 1`). Do not couple the condition with the
orthogonal dimension count (e.g., `tile_rows == 1`).

**Why.** Per the AV1 specification, minimum tile width restrictions apply only
to inner tile columns and exempt the rightmost column. When a frame has only one
column (`tile_cols == 1`), that column is the rightmost column regardless of how
many tile rows exist; requiring `tile_rows == 1` erroneously rejects valid
multi-row, single-column frames narrower than the minimum tile width.

## Examples

Bad:

```cpp
// Erroneously requires a single tile in both dimensions
if (cm->tiles.cols == 1 && cm->tiles.rows == 1) {
  return 1;
}
return ((cm->tiles.min_inner_width << MI_SIZE_LOG2) >= 64);
```

Good:

```cpp
// Bypasses column width check solely when there is only one column
if (cm->tiles.cols == 1) {
  return 1;
}
return ((cm->tiles.min_inner_width << MI_SIZE_LOG2) >= 64);
```
