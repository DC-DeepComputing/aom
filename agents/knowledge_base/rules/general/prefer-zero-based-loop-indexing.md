---
name: libaom/prefer-zero-based-loop-indexing
description: When iterating over a fixed count of elements or buffer rows, use 0-based loop indexing rather than non-zero base offsets and oversized arrays
rubric: readability
severity: nit
---

**Rule.** When iterating over a fixed number of elements, rows, or buffer
entries, use standard 0-based loop indexing (`for (int i = 0; i < count; ++i)`)
and size local arrays or buffers to the exact count needed. Do not use arbitrary
non-zero base index ranges (such as `for (int i = 2; i < 5; ++i)`) that require
allocating oversized arrays with unused initial slots.

**Why.** 0-based indexing is standard idiomatic C/C++ and ensures arrays and
buffers are sized tightly to their active elements without wasting stack memory
or cache lines on unused prefix slots. It also simplifies boundary reasoning and
minimizes off-by-one errors when refactoring.

## Examples

Bad:

```cpp
// Oversized array with unused slots [0] and [1] due to non-zero loop indexing
uint32x4_t vsrc[5][2];
for (int i = 2; i < 5; ++i) {
  load_and_pad(src, vsrc[i], col, block_width);
  src += SSE_STRIDE;
}
```

Good:

```cpp
// Tightly sized array using standard 0-based loop indexing
uint32x4_t vsrc[3][2];
for (int i = 0; i < 3; ++i) {
  load_and_pad(src, vsrc[i], col, block_width);
  src += SSE_STRIDE;
}
```
