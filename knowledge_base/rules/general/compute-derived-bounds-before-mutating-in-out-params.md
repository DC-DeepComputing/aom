---
name: libaom/compute-derived-bounds-before-mutating-in-out-params
description: When updating in-out pointer parameters in place, compute derived bounds and helper values before mutating the parameter to eliminate redundant temporary variables
rubric: readability
severity: nit
---

**Rule.** When a function mutates an in-out pointer parameter in place, compute
any derived bounds, clamping limits, or helper values that depend on its initial
value before modifying the parameter. Avoid creating redundant temporary
variables that exist solely to preserve the original value across intermediate
mutations.

**Why.** Deriving limits directly from the in-out parameter prior to mutation
keeps the computation straightforward and eliminates unnecessary temporary
variables that add clutter and state tracking.

## Examples

Bad:

```cpp
// Redundant temporary variable 'orig_dim' used solely to preserve input value
const int orig_dim = *dim;
*dim = (*dim * SCALE_NUMERATOR + denom / 2) / denom;
const int min_dim = AOMMIN(16, orig_dim);
*dim = AOMMAX(*dim, min_dim);
```

Good:

```cpp
// Compute derived bound from '*dim' upfront before mutating it in place
const int min_dim = AOMMIN(16, *dim);
*dim = (*dim * SCALE_NUMERATOR + denom / 2) / denom;
*dim = AOMMAX(*dim, min_dim);
```
