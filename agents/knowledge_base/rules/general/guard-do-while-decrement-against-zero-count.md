---
name: libaom/guard-do-while-decrement-against-zero-count
description: When writing do-while loops with decrement conditions, check while ((--count) > 0) or use pre-tested loops to prevent infinite loops on zero counts
rubric: correctness
severity: blocking
---

**Rule.** Do not use bare decrement conditions in `do { ... } while (--count)`
loops when the iteration count can be zero or non-positive. Guard decrement
conditions with `while ((--count) > 0)` or use pre-condition loops (`for`,
`while (count-- > 0)`) so zero counts do not decrement past zero and loop
infinitely.

**Why.** A `do { ... } while` loop executes its body before testing the
condition. When the initial count is zero, `--count` evaluates to `-1` (or
underflows to a large unsigned value), which evaluates as true and causes an
infinite loop.

## Examples

Bad:

```cpp
do {
  convolve_row_avx2(src_ptr, coeffs, dst_ptr);
  src_ptr += src_stride;
  dst_ptr += dst_stride;
} while (--h);
```

Good:

```cpp
do {
  convolve_row_avx2(src_ptr, coeffs, dst_ptr);
  src_ptr += src_stride;
  dst_ptr += dst_stride;
} while ((--h) > 0);
```
