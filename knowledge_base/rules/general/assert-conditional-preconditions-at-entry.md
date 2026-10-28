---
name: libaom/assert-conditional-preconditions-at-entry
description: When asserting function-level parameter constraints or preconditions, place assertions at function entry—using assert(IMPLIES(cond, expr)) for conditional invariants—with clarifying comments
rubric: readability
severity: nit
---

**Rule.** Assert function-level preconditions and parameter constraints at the
entry of the function rather than repeatedly inside inner loops or deep
branches. Use assert(IMPLIES(condition, requirement)) to express conditional
invariants, and accompany non-obvious constraints with explanatory comments.

**Why.** Placing precondition assertions at function entry establishes and
validates the function contract upfront, avoids redundant assertion evaluations
across loop iterations, and documents conditional parameter relationships for
maintainers and static analyzers.

## Examples

Bad:

```cpp
for (int lvl2_idx = 0; lvl2_idx < 4; lvl2_idx++) {
  if (is_key_frame) {
    assert(dst_buf == NULL);
    // ...
  }
}
```

Good:

```cpp
// dst_buf pointer is not used for key frames, so it should be NULL.
assert(IMPLIES(is_key_frame, dst_buf == NULL));

for (int lvl2_idx = 0; lvl2_idx < 4; lvl2_idx++) {
  if (is_key_frame) {
    // ...
  }
}
```
