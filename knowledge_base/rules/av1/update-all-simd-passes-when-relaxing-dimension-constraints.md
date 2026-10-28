---
name: libaom/av1/update-all-simd-passes-when-relaxing-dimension-constraints
description: When relaxing dimension alignment constraints in multi-pass SIMD/DSP kernels, ensure all processing stages and passes handle remainder rows and columns
rubric: correctness
severity: blocking
---

**Rule.** When relaxing dimension alignment requirements (such as allowing
heights or widths that are not multiples of 4 or 8) in multi-stage or separable
2D SIMD/DSP functions, ensure every processing stage (including both horizontal
and vertical passes) is updated to handle remainder rows or columns. Avoid fixed
decrement loops (such as `height -= 4; while (height > 0)`) that step past zero
on unaligned dimensions; check loop bounds with threshold conditions (e.g.,
`while (height > 3)` or `while (w > 3)`) and provide remainder handling loops
for the remaining lines.

**Why.** In multi-pass or separable SIMD algorithms, intermediate and output
buffers rely on all passes processing matching dimensions. If a horizontal or
vertical stage retains fixed decrement steps assuming divisible dimensions,
unaligned dimensions cause loop counters to underflow past zero, leading to
unbounded loop execution and out-of-bounds heap buffer overflows.

## Examples

Bad:

```cpp
/* Relaxed height requirement, but horizontal pass retains fixed step */
do {
  /* Process 4 rows with SIMD */
  ...
  height -= 4;
} while (height > 0);  /* Underflows past 0 if height is not a multiple of 4 */
```

Good:

```cpp
/* Process 4 rows per iteration while at least 4 rows remain */
while (height > 3) {
  /* Process 4 rows with SIMD */
  ...
  height -= 4;
}
/* Handle remaining 1 to 3 rows */
while (height-- != 0) {
  /* Process 1 row with SIMD */
  ...
}
```
