---
name: libaom/prefer-integer-loop-counters-over-floating-point
description: When iterating over floating-point ranges or stepped intervals, use integer loop counters and compute offsets inside the loop instead of accumulating floating-point variables
rubric: readability
severity: nit
---

**Rule.** Use integer loop index variables (e.g., `for (int i = start; i <= end;
++i)`) instead of floating-point iteration variables and step additions (`x +=
step`). Compute floating-point offsets or coordinates inside the loop body by
multiplying the integer index by the step size (`i * step`).

**Why.** Directly incrementing floating-point loop counters accumulates
numerical rounding errors on each iteration, causing drift and potentially
skipping or adding loop iterations due to imprecise boundary comparisons.
Integer index counters guarantee exact iteration counts and eliminate
accumulated rounding errors.

## Examples

Bad:

```cpp
// Anti-pattern: Floating-point iteration variable accumulates rounding error.
for (delta_phi = -30 * search_step_phi; delta_phi <= 30 * search_step_phi;
     delta_phi += search_step_phi) {
  process_angle(delta_phi);
}
```

Good:

```cpp
// Recommended: Integer counter with exact step multiplication inside loop.
for (int i = -30; i <= 30; ++i) {
  const double delta_phi = i * search_step_phi;
  process_angle(delta_phi);
}
```
