---
name: libaom/check-floating-point-magnitude-near-zero
description: When testing whether a floating-point determinant, divisor, or delta is near zero against an epsilon threshold, check absolute magnitude with fabs()
rubric: correctness
severity: should_fix
---

**Rule.** When checking whether a signed floating-point value (such as a matrix
determinant, divisor, denominator, or residual) is degenerate or near zero
against an epsilon threshold, check its absolute magnitude using `fabs()` or
`fabsf()` (e.g., `fabs(det) < 1e-5`). Do not use one-sided comparisons like `det
< eps` or `val < 1e-5`, which incorrectly treat all valid negative values as
degenerate.

**Why.** One-sided comparisons like `val < eps` evaluate to true for all
negative numbers, erroneously rejecting valid non-zero values such as invertible
matrices with negative determinants or negative divisors.

## Examples

Bad:

```cpp
static INLINE bool invert_2x2(const double *M, double *M_inv) {
  double det = (M[0] * M[3]) - (M[1] * M[2]);
  // Bad: one-sided check treats valid negative determinants as singular.
  if (det < 1e-5) {
    return false;
  }
  const double det_inv = 1.0 / det;
  M_inv[0] = M[3] * det_inv;
  M_inv[1] = -M[1] * det_inv;
  M_inv[2] = -M[2] * det_inv;
  M_inv[3] = M[0] * det_inv;
  return true;
}
```

Good:

```cpp
static INLINE bool invert_2x2(const double *M, double *M_inv) {
  double det = (M[0] * M[3]) - (M[1] * M[2]);
  // Good: checks magnitude against near-zero epsilon threshold.
  if (fabs(det) < 1e-5) {
    return false;
  }
  const double det_inv = 1.0 / det;
  M_inv[0] = M[3] * det_inv;
  M_inv[1] = -M[1] * det_inv;
  M_inv[2] = -M[2] * det_inv;
  M_inv[3] = M[0] * det_inv;
  return true;
}
```
