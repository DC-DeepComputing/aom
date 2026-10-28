---
name: libaom/av1/reserve-mi-prefix-for-mode-info-units
description: When naming coordinates or dimensions in AV1, reserve the mi_ prefix for mode info grid units (4x4 pixels) and use descriptive names for pixel units
rubric: readability
severity: should_fix
---

**Rule.** Reserve the `mi_` prefix (such as `mi_row`, `mi_col`, `mi_x`, `mi_y`,
`mi_width`, `mi_height`) exclusively for coordinates, offsets, and dimensions
expressed in mode info grid block units (where 1 unit equals `MI_SIZE` / 4
pixels). Do not use `mi_` prefixes for pixel-unit values; name pixel coordinates
using explicit pixel terms (e.g., `x`, `y`, `center_x`, `center_y`, `row`,
`col`, `width`, `height`).

**Why.** In AV1 and libaom, mode info (`MB_MODE_INFO`) blocks operate on a 4x4
pixel grid (`MI_SIZE`). Using the `mi_` prefix for pixel-scale coordinates
misleadingly implies block-grid units, creating confusion about scaling factors
and risking unit-mismatch defects during coordinate transformations.

## Examples

Bad:

```cpp
// Anti-pattern: Using mi_ prefix for pixel coordinates
void av1_warp_rotation(MB_MODE_INFO *mi, int8_t rotation, int mi_x, int mi_y) {
  const int radius_x = mi_x - block_center_x;
  const int radius_y = mi_y - block_center_y;
  // ...
}
```

Good:

```cpp
// Idiomatic: mi_ prefix reserved for MI block units, pixel descriptors for pixel units
void av1_warp_rotation(MB_MODE_INFO *mi, int8_t rotation, int center_x, int center_y) {
  const int radius_x = center_x - block_center_x;
  const int radius_y = center_y - block_center_y;
  // ...
}
```
