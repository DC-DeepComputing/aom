---
name: libaom/order-bounds-checks-before-predicates-in-conditions
description: When writing loop conditions or compound guards with bounds limits and value predicates, place the bounds check first before indexing or function calls
rubric: readability
severity: nit
---

**Rule.** In loop conditions and compound guards that test both an index or
bounds limit and a value or computation predicate, place the bounds check first
(e.g., `index < max && predicate(index)`). Do not place function calls, array
accesses, or expensive predicates before the bounds test.

**Why.** Placing the bounds check first leverages short-circuit logical-AND (&&)
evaluation to avoid executing redundant function calls, complex computations, or
out-of-bounds array lookups once the terminal limit is reached.

## Examples

Bad:

```cpp
// Bad: Function call is evaluated even when qindex exceeds MAXQ.
while (av1_convert_qindex_to_q(qindex, bit_depth) < q && qindex < MAXQ) {
  qindex++;
}
```

Good:

```cpp
// Good: Bounds check is evaluated first, short-circuiting when qindex reaches MAXQ.
while (qindex < MAXQ && av1_convert_qindex_to_q(qindex, bit_depth) < q) {
  qindex++;
}
```
