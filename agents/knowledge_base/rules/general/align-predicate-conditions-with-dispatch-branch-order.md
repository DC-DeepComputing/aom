---
name: libaom/align-predicate-conditions-with-dispatch-branch-order
description: When writing capability-check or fast-path predicates mirroring multi-branch dispatch logic, align condition order and arithmetic phrasing with dispatch branches
rubric: readability
severity: nit
---

**Rule.** When writing a capability-check or validation predicate function that
mirrors multi-branch dispatch logic, align the order of conditional clauses and
use identical arithmetic expression phrasing across both the predicate and the
dispatch handler. Do not reorder condition clauses or alternate between
different algebraic forms (such as `2 * dst == src` vs `dst * 2 == src`) between
the predicate and the dispatch `if`/`else if` branches.

**Why.** Keeping clause ordering and expression syntax identical between
capability predicates and dispatch functions allows reviewers and maintainers to
visually verify that all supported modes match, preventing subtle logic drift or
missed dispatch paths.

## Examples

Bad:

```cpp
// Dispatch branches evaluate 2:1, 4:1, 4:3, 1:2 in order:
// Predicate uses arbitrary ordering and inverted algebraic phrasing:
static inline bool has_scaler_ssse3(int src_w, int src_h, int dst_w, int dst_h) {
  return (dst_w * 4 == src_w && dst_h * 4 == src_h) ||
         (dst_w * 2 == src_w && dst_h * 2 == src_h) ||
         (dst_w * 4 == src_w * 3 && dst_h * 4 == src_h * 3) ||
         (dst_w == src_w * 2 && dst_h == src_h * 2);
}
```

Good:

```cpp
// Predicate matches dispatch branch order (2:1, 4:1, 4:3, 1:2) and syntax:
static inline bool has_scaler_ssse3(int src_w, int src_h, int dst_w, int dst_h) {
  return (2 * dst_w == src_w && 2 * dst_h == src_h) ||
         (4 * dst_w == src_w && 4 * dst_h == src_h) ||
         (4 * dst_w == 3 * src_w && 4 * dst_h == 3 * src_h) ||
         (dst_w == src_w * 2 && dst_h == src_h * 2);
}
```
