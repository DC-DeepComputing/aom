---
name: libaom/document-mathematical-index-transformations
description: When implementing non-trivial index transformations, packing or unpacking helpers, or combinatorial mappings, document input/output domains, parameter constraints, and ordering schemes
rubric: readability
severity: nit
---

**Rule.** When defining or updating helper functions that perform non-trivial
index conversions, packing or unpacking operations (such as `single2comb` or
`comb2single`), combinatorial mappings, or coordinate flattenings, document the
expected input and output domains, parameter constraints, enumeration ordering,
and underlying indexing arithmetic in comments above the function. Additionally,
include comments within non-obvious mapping loops or indexing arithmetic to
explain loop invariants, progression steps, and boundary conditions.

**Why.** Non-linear mathematical mappings and combinatorial index conversions
(such as flattening index pairs or triangular coordinate packing/unpacking) are
difficult to verify from arithmetic formulas alone. Explicit documentation of
domain bounds, ordering sequences, parameter constraints, and mapping steps
prevents off-by-one errors and clarifies indexing invariants across callers.

## Examples

Bad:

```cpp
// Bad: Packing helper without domain bounds or ordering explanation.
static INLINE int8_t single2comb(int n, const int8_t *const rf) {
  assert(rf[0] < n && rf[1] < n && rf[1] > rf[0]);
  int off = n * rf[0] - rf[0] * (rf[0] + 1) / 2;
  return off + rf[1] - rf[0] - 1;
}

// Bad: Unpacking helper without comments on layout, ranges, or loop logic.
static INLINE void comb2single(int n, int8_t combindex, int8_t *rf) {
  assert(combindex < n * (n - 1) / 2);
  int i = n - 1, j = n - 1;
  rf[0] = 0;
  while (i <= combindex) {
    rf[0]++;
    j--;
    i += j;
  }
  rf[1] = combindex - i + j + rf[0] + 1;
}
```

Good:

```cpp
// Converts a pair of distinct indices (rf) in [0, n-1] (rf[0] < rf[1])
// to a combined index in [0, n*(n-1)/2 - 1] in upper-triangular order:
// (0,1), (0,2)...(0,n-1), (1,2)...(1,n-1), ..., (n-2,n-1).
static INLINE int8_t single2comb(int n, const int8_t *const rf) {
  assert(rf[0] < n && rf[1] < n && rf[1] > rf[0]);
  int off = n * rf[0] - rf[0] * (rf[0] + 1) / 2;
  return off + rf[1] - rf[0] - 1;
}

// Converts a combined index in [0, n*(n-1)/2 - 1] to a pair of single
// ref indices rf[0], rf[1] in [0, n-1] where rf[0] < rf[1].
static INLINE void comb2single(int n, int8_t combindex, int8_t *rf) {
  assert(combindex < n * (n - 1) / 2);
  int i = n - 1, j = n - 1;
  rf[0] = 0;
  // Starting from n - 1, reduce row length by 1 until combindex < i.
  while (i <= combindex) {
    rf[0]++;
    j--;
    i += j;
  }
  rf[1] = combindex - i + j + rf[0] + 1;
  assert(rf[1] > rf[0]);
}
```
