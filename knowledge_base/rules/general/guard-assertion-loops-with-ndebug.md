---
name: libaom/guard-assertion-loops-with-ndebug
description: 'When asserting conditions across array elements, iterate in a loop guarded by #if !defined(NDEBUG) or #ifndef NDEBUG instead of manually unrolling assertions'
rubric: readability
severity: should_fix
---

**Rule.** When asserting invariants across array elements or repetitive
structure members, iterate over the elements in a loop rather than manually
unrolling multiple assertion statements. Enclose assertion-only loops in an `#if
!defined(NDEBUG)` (or `#ifndef NDEBUG`) preprocessor guard so release builds do
not execute empty loop iterations.

**Why.** Iterating over array elements in a loop avoids verbose boilerplate and
ensures consistent invariant checking across all entries. Wrapping assertion
loops in `#if !defined(NDEBUG)` or `#ifndef NDEBUG` ensures that when `assert()`
is compiled out in release builds, the loop construct itself is eliminated from
the binary without runtime overhead or unused-variable warnings.

## Examples

Bad:

```cpp
assert((pc_tree->horizontala[0] == NULL) &&
       (pc_tree->horizontala[1] == NULL) &&
       (pc_tree->horizontala[2] == NULL));
assert((pc_tree->horizontalb[0] == NULL) &&
       (pc_tree->horizontalb[1] == NULL) &&
       (pc_tree->horizontalb[2] == NULL));
assert((pc_tree->verticala[0] == NULL) && (pc_tree->verticala[1] == NULL) &&
       (pc_tree->verticala[2] == NULL));
```

Good:

```cpp
#if !defined(NDEBUG)
for (int i = 0; i < 3; ++i) {
  assert(pc_tree->horizontala[i] == NULL);
  assert(pc_tree->horizontalb[i] == NULL);
  assert(pc_tree->verticala[i] == NULL);
}
#endif
```
