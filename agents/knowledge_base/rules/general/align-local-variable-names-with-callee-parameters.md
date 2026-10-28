---
name: libaom/align-local-variable-names-with-callee-parameters
description: When declaring or computing local variables passed into a helper or callee function, align local variable names with the corresponding callee parameter names
rubric: readability
severity: nit
---

**Rule.** When computing or declaring local variables that are passed into
helper or callee functions, align local variable names with the corresponding
parameter names of the callee function. Maintain consistent naming prefixes and
terminology (such as using an n_ prefix for counts or quantities) between caller
arguments and callee parameters.

**Why.** Matching caller local variable names to callee parameter names
clarifies the purpose and units of intermediate values, avoids ambiguity at
function call sites, and makes code easier to follow across call boundaries.

## Examples

Bad:

```cpp
// Local variable names diverge from callee parameter names:
const int top_right_px = have_top_right > 0 ? AOMMIN(txwpx, xr) : have_top_right;
const int bottom_left_px = have_bottom_left > 0 ? AOMMIN(txhpx, yd) : have_bottom_left;
build_directional_and_filter_intra_predictors(
    xd, mode, tx_size, disable_edge_filter, top_px, top_right_px, left_px,
    bottom_left_px);
```

Good:

```cpp
// Local variable names match callee parameter names:
const int n_topright_px = have_top_right > 0 ? AOMMIN(txwpx, xr) : have_top_right;
const int n_bottomleft_px = have_bottom_left > 0 ? AOMMIN(txhpx, yd) : have_bottom_left;
build_directional_and_filter_intra_predictors(
    xd, mode, tx_size, disable_edge_filter, n_top_px, n_topright_px, n_left_px,
    n_bottomleft_px);
```
