---
name: libaom/av1/flatten-row-invariant-intermediate-buffers
description: When intermediate multi-dimensional calculations produce row-invariant or column-invariant results, use a 1D array instead of allocating and redundantly populating a 2D buffer
rubric: safety
severity: should_fix
---

**Rule.** When an intermediate buffer stores values that do not vary across rows
or columns (such as identical per-column statistics computed across all rows),
declare a 1D array sized to the active dimension rather than allocating and
filling a full 2D array. Eliminate redundant outer loops that repeatedly store
identical values across invariant dimensions.

**Why.** Allocating and populating a full 2D intermediate buffer when values are
uniform across rows wastes stack and cache space and incurs unnecessary store
operations in redundant loops.

## Examples

Bad:

```cpp
// Redundant 2D buffer and repeated stores across rows for column sums
uint32_t acc_5x5[BH][BW];
for (int row = 0; row < block_height; row++) {
  for (int col = 0; col < block_width; col += 4) {
    vst1q_u32(&acc_5x5[row][col], vsum);
  }
}
```

Good:

```cpp
// Sized as a 1D buffer populated once per column
uint32_t acc_5x5[BW];
for (int col = 0; col < block_width; col += 4) {
  vst1q_u32(&acc_5x5[col], vsum);
}
```
