---
name: libaom/av1/validate-mode-info-coordinates-against-grid-dimensions
description: When asserting or bounds-checking mode-info block coordinates, validate them against mode-info grid dimensions (mi_rows, mi_cols) rather than unscaled pixel dimensions
rubric: correctness
severity: blocking
---

**Rule.** When asserting, validating, or bounds-checking mode-info block
coordinates (such as mi_row, mi_col, curr_mi_row, or curr_mi_col), compare them
against mode-info grid dimensions (such as cm->mi_params.mi_rows and
cm->mi_params.mi_cols). Do not compare mode-info block coordinates directly
against unscaled pixel buffer dimensions (dst.width, dst.height) without
converting units (e.g., via << MI_SIZE_LOG2).

**Why.** Mode-info (MI) coordinates index 4x4 block units across the mode-info
grid (mi_grid_base), whereas plane and frame buffer dimensions are measured in
pixels (MI_SIZE pixels per unit). Comparing block coordinates directly against
pixel dimensions creates a 4x unit scale mismatch, causing false assertion
failures or allowing out-of-bounds grid accesses to go undetected.

## Examples

Bad:

```cpp
// Scale mismatch: comparing 4x4 block coordinates against pixel dimensions
assert(curr_mi_col < plane_ptr->dst.width);
assert(curr_mi_row < plane_ptr->dst.height);
```

Good:

```cpp
// Correct: comparing block coordinates against mode-info grid dimensions
assert(curr_mi_col < cm->mi_params.mi_cols);
assert(curr_mi_row < cm->mi_params.mi_rows);

// Alternatively, scale block coordinates to pixels before comparing
assert((curr_mi_col << MI_SIZE_LOG2) < (uint32_t)plane_ptr->dst.width);
assert((curr_mi_row << MI_SIZE_LOG2) < (uint32_t)plane_ptr->dst.height);
```
