---
name: libaom/avoid-unconditional-result-assignment-in-search-loops
description: When searching for an index or value in a loop with a default fallback, assign the result variable only inside the match condition rather than unconditionally
rubric: correctness
severity: nit
---

**Rule.** When searching for an index or value in a loop that initializes a
default or fallback result, do not unconditionally assign the loop variable to
the result variable before evaluating the match condition. Assign the result
variable only inside the conditional match block (or return early from a helper)
so that failing to find a match preserves the pre-initialized fallback value.

**Why.** Unconditionally assigning the loop variable at the start of an
iteration immediately overwrites any pre-initialized default value. If the loop
completes without finding any matching element, the result variable incorrectly
retains the last inspected loop item instead of the intended fallback value.

## Examples

Bad:

```cpp
int target_index = MAXQ;
for (int i = MINQ; i < MAXQ; ++i) {
  target_index = i;
  if (av1_convert_qindex_to_q(i, bit_depth) >= qtarget) {
    break;
  }
}
```

Good:

```cpp
int target_index = MAXQ;
for (int i = MINQ; i < MAXQ; ++i) {
  if (av1_convert_qindex_to_q(i, bit_depth) >= qtarget) {
    target_index = i;
    break;
  }
}
```
