---
name: libaom/avoid-redundant-outer-loop-wrappers
description: When iterating over elements or candidates, avoid wrapping inner loops in redundant outer loops (such as do-while) with identical termination conditions
rubric: readability
severity: nit
---

**Rule.** Do not wrap an inner loop in an outer loop (such as `do { ... } while
(...)`) when the inner loop already tests the same termination conditions and
iteration bounds. Rely directly on the inner loop's condition and loop step
rather than bouncing between nested loop levels with internal `break`
statements.

**Why.** Wrapping an inner loop in a redundant outer loop with duplicate
termination conditions adds unnecessary nesting depth and convoluted control
flow without altering execution semantics, making loop termination logic harder
to audit.

## Examples

Bad:

```cpp
do {
  for (; idx_left < count_left && *refmv_count < ref_mv_limit; ++idx_left) {
    if (check_cand(cand, ref_frame)) {
      ++*refmv_count;
      break;
    }
  }
  if (idx_left >= count_left) break;
} while (*refmv_count < ref_mv_limit);
```

Good:

```cpp
for (; idx_left < count_left && *refmv_count < ref_mv_limit; ++idx_left) {
  if (check_cand(cand, ref_frame)) {
    ++*refmv_count;
  }
}
```
