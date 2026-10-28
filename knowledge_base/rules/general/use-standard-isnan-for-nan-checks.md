---
name: libaom/use-standard-isnan-for-nan-checks
description: When checking floating-point values for NaN, use standard isnan() from <math.h> rather than arithmetic tricks or self-comparisons
rubric: readability
severity: should_fix
---

**Rule.** When checking floating-point values for NaN (Not-a-Number), use the
standard isnan() macro from <math.h> (or std::isnan() in C++) rather than ad-hoc
arithmetic tricks (such as x * 0.0 != 0.0) or self-comparisons (x != x).

**Why.** Ad-hoc arithmetic tricks and self-comparisons to detect NaN are obscure
and can be misoptimized or eliminated by compilers under fast-math optimization
flags. The standard isnan() macro clearly expresses intent and conforms to IEEE
754 semantics across all supported compilers.

## Examples

Bad:

```cpp
// Anti-pattern: arithmetic trick or self-comparison to check for NaN
for (int i = 0; i < n; ++i) {
  if (eqns->x[i] * 0.0 != 0.0) {
    return 0;
  }
}
```

Good:

```cpp
// Recommended: standard isnan() macro from <math.h>
for (int i = 0; i < n; ++i) {
  if (isnan(eqns->x[i])) {
    return 0;
  }
}
```
