---
name: libaom/document-parameter-assumptions-in-assertions
description: When adding or modifying assertions or invariant checks that rely on parameter values configured elsewhere, document the assumption and source.
rubric: readability
severity: nit
---

**Rule.** When an assertion, precondition check, or mathematical invariant in a
helper function relies on a specific parameter value, formula, or configuration
established elsewhere, explicitly document that assumption and cross-reference
where it is set. Avoid unexplained concrete numeric thresholds when the
surrounding derivation or algorithm uses symbolic variables.

**Why.** Hardcoded numeric bounds in assertions are difficult to verify when the
invariant depends on parameters configured in other functions. Documenting the
assumed parameter values and cross-referencing their source connects the
algorithm derivation to the concrete check and prevents confusion during
maintenance.

## Examples

Bad:

```cpp
// Note: Due to how the least-squares matrix is constructed, all
// eigenvalues are >= 0, so det M >= 0.
// The regularization term `+ k * I` further ensures det M >= k^2.
// Therefore we do not have to worry about non-invertible matrices here.
static inline void invert_2x2(const double *M, double *M_inv) {
  double det = (M[0] * M[3]) - (M[1] * M[2]);
  assert(det >= 1);
  ...
}
```

Good:

```cpp
// Note: Due to how the least-squares matrix is constructed, all
// eigenvalues are >= 0, so det M >= 0.
// The regularization term `+ k * I` further ensures det M >= k^2.
// As configured in compute_flow_matrix(), here we use k = 1, so det M >= 1.
// Therefore we do not have to worry about non-invertible matrices here.
static inline void invert_2x2(const double *M, double *M_inv) {
  double det = (M[0] * M[3]) - (M[1] * M[2]);
  assert(det >= 1);
  ...
}
```
