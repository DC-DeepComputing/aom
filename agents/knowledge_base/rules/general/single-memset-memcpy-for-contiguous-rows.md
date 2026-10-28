---
name: libaom/single-memset-memcpy-for-contiguous-rows
description: When clearing or copying contiguous 2D buffer rows where stride equals width, use a single memset or memcpy call instead of looping row by row
rubric: safety
severity: should_fix
---

**Rule.** When clearing or copying contiguous multi-row 2D buffers or
rectangular regions where the row stride equals the row width (no stride padding
between rows), use a single `memset` or `memcpy` call covering the entire buffer
range (e.g. `rows * cols * sizeof(...)`) instead of looping over rows.

**Why.** Invoking `memset` or `memcpy` inside a loop over contiguous memory
introduces redundant loop branching and function call overhead. A single
invocation enables the compiler or runtime library to optimize bulk vector
memory operations across the entire contiguous block.

## Examples

Bad:

```cpp
// Redundant row-by-row memset over contiguous memory
for (; row_start < txfm_size_row; row_start++) {
  memset(buf_ptr, 0, sizeof(int32_t) * txfm_size_col);
  buf_ptr += txfm_size_col;
}
```

Good:

```cpp
// Single memset across all contiguous rows
const int remaining_rows = txfm_size_row - row_start;
memset(buf_ptr, 0, sizeof(int32_t) * txfm_size_col * remaining_rows);
```
