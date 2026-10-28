---
name: libaom/use-relational-comparisons-in-qsort-comparators
description: When implementing comparator callbacks for qsort or bsearch, use relational comparisons instead of arithmetic subtraction to prevent integer overflow
rubric: correctness
severity: nit
---

**Rule.** In comparator callbacks (such as for qsort or bsearch), do not compute
ordering by subtracting element values (*a - *b). Use relational comparisons
like (a > b) - (a < b) or explicit branching (-1, 0, 1), and preserve const
qualification when casting const void * parameters (const Type *).

**Why.** Direct arithmetic subtraction *a - *b in comparator callbacks causes
integer overflow or underflow when differences exceed integer range bounds (or
produces incorrect sign behavior for unsigned types), leading to incorrect
sorting order or undefined behavior. Relational comparisons avoid overflow, and
casting to const Type * ensures const correctness.

## Examples

Bad:

```cpp
static int variance_comp(const void *a, const void *b) {
  // Bad: subtraction can overflow or misbehave on unsigned integers,
  // and casting discards const qualifier.
  return *(int *)a - *(int *)b;
}
```

Good:

```cpp
static int variance_comp(const void *a, const void *b) {
  const unsigned int val_a = *(const unsigned int *)a;
  const unsigned int val_b = *(const unsigned int *)b;

  return (val_a > val_b) - (val_a < val_b);
}
```
