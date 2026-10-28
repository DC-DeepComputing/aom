---
name: libaom/nest-dependent-conditionals-to-simplify-checks
description: When conditional calculations depend on an enclosing branch's condition, move them inside the block to eliminate redundant predicate checks and narrow variable scope
rubric: readability
severity: nit
---

**Rule.** When secondary conditional logic or variable computations depend on an
outer branch condition whose predicate already subsumes part of the check, move
the calculation and inner conditional inside that branch. Eliminate redundant
compound predicate checks in the nested condition and declare intermediate
variables in the innermost scope where they are used.

**Why.** Placing dependent calculations and checks in a separate top-level
conditional forces redundant evaluation of compound conditions that are already
logically guaranteed by earlier branches. Moving the logic inside the enclosing
block simplifies the inner predicate expression, avoids unneeded computations on
bypass paths, and restricts variable lifetimes to their narrowest scope.

## Examples

Bad:

```cpp
TX_SIZE pv_ts;
if (!is_first_block || coord) {
  pv_ts = get_transform_size(xd, mi_prev, pv_row, pv_col, plane);
}
if (is_first_block && coord) {
  *min_dim = is_vert ? tx_size_high[pv_ts] : tx_size_wide[pv_ts];
}
```

Good:

```cpp
if (!is_first_block || coord) {
  const TX_SIZE pv_ts =
      get_transform_size(xd, mi_prev, pv_row, pv_col, plane);
  if (is_first_block) {
    *min_dim = is_vert ? tx_size_high[pv_ts] : tx_size_wide[pv_ts];
  }
}
```
