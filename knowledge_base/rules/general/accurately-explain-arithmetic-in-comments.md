---
name: libaom/accurately-explain-arithmetic-in-comments
description: When explaining mathematical operations or rounding and truncation effects in code comments, accurately state the underlying arithmetic mechanism rather than citing misleading factors
rubric: readability
severity: should_fix
---

**Rule.** When documenting mathematical operations, arithmetic formulas, or
rounding and truncation effects in code comments, accurately state the
underlying arithmetic mechanism (such as integer right-shift truncation on odd
values discarding 0.5) rather than attributing behavior to misleading or
unrelated factors. Keep explanations mathematically precise regarding how
operations affect edge cases, odd versus even values, and small magnitudes.

**Why.** Inaccurate or imprecise explanations in code comments confuse reviewers
and future maintainers about why calculations produce specific results. Stating
the exact mathematical mechanism clarifies the arithmetic cause of rounding or
truncation effects and prevents faulty assumptions during algorithm tuning and
refactoring.

## Examples

Bad:

```cpp
// Please note that the ">> 1" often reduces strengths by more than a half in
// practice, due to how few strength levels there are (0-15 pri, 0-3 sec).
int pri_strength = cdef_pri_damping >> 1;
```

Good:

```cpp
// Please note that ">> 1" truncates odd quotients, discarding 0.5 which
// represents a significant fraction when strength values are small.
int pri_strength = cdef_pri_damping >> 1;
```
