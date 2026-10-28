---
name: libaom/assert-buffer-capacity-before-grid-indexing
description: When iterating over multidimensional grid dimensions and indexing into flat allocated arrays, assert that allocated buffer capacity matches expected dimension products
rubric: correctness
severity: should_fix
---

**Rule.** When iterating over multidimensional grids (such as tile or superblock
rows and columns) and indexing into flat allocated arrays or buffers via
computed linear offsets (row * cols + col), assert that the allocated buffer
capacity matches or accommodates the expected grid dimension product (e.g.,
assert(cpi->allocated_tiles == tile_cols * tile_rows)) before entering the loop.
Verify that capacity tracking fields are checked before executing flat index
arithmetic to catch allocation desynchronization early.

**Why.** Multi-dimensional loops compute flat array indices based on current
runtime grid dimensions. If the underlying buffer was allocated under earlier or
mismatched dimension constraints, missing assertions allow silent out-of-bounds
memory accesses and heap corruption during debug execution.

## Examples

Bad:

```cpp
// Allocate memory for row based multi-threading
for (tile_row = 0; tile_row < tile_rows; tile_row++) {
  for (tile_col = 0; tile_col < tile_cols; tile_col++) {
    int tile_index = tile_row * tile_cols + tile_col;
    TileDataEnc *const this_tile = &cpi->tile_data[tile_index];
    // Process tile data...
  }
}
```

Good:

```cpp
// Allocate memory for row based multi-threading
assert(cpi->allocated_tiles == tile_cols * tile_rows);
for (tile_row = 0; tile_row < tile_rows; tile_row++) {
  for (tile_col = 0; tile_col < tile_cols; tile_col++) {
    int tile_index = tile_row * tile_cols + tile_col;
    TileDataEnc *const this_tile = &cpi->tile_data[tile_index];
    // Process tile data...
  }
}
```
