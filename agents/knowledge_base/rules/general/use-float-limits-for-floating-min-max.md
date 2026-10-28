---
name: libaom/use-float-limits-for-floating-min-max
description: When initializing float or double min/max tracking variables, use floating-point limits like DBL_MAX or FLT_MAX instead of integer constants like INT_MAX
rubric: correctness
severity: should_fix
---

**Rule.** When initializing variables or accumulator targets that track
floating-point minimums and maximums (of type float or double), use
type-appropriate floating-point limits (such as DBL_MAX, -DBL_MAX, FLT_MAX,
-FLT_MAX, or std::numeric_limits<T>) or initialize directly from the first
element. Do not initialize floating-point tracking variables with integer limit
macros (such as INT_MAX or INT_MIN).

**Why.** Using integer limits like INT_MAX or INT_MIN for float or double
variables creates a type mismatch and restricts tracking to integer ranges
(~2×10^9), causing incorrect comparisons or missed bounds updates when
floating-point values exceed integer limits. Type-appropriate floating-point
limits ensure correctness across the full representable range.

## Examples

Bad:

```cpp
// Incorrect: Using integer limits for double min/max tracking.
void find_min_max(const double *buf, int n, double *min_val, double *max_val) {
  *min_val = INT_MAX;
  *max_val = INT_MIN;
  for (int i = 0; i < n; ++i) {
    if (buf[i] < *min_val) *min_val = buf[i];
    if (buf[i] > *max_val) *max_val = buf[i];
  }
}
```

Good:

```cpp
// Correct: Using type-appropriate floating-point limits.
void find_min_max(const double *buf, int n, double *min_val, double *max_val) {
  *min_val = DBL_MAX;
  *max_val = -DBL_MAX;
  for (int i = 0; i < n; ++i) {
    if (buf[i] < *min_val) *min_val = buf[i];
    if (buf[i] > *max_val) *max_val = buf[i];
  }
}
```
