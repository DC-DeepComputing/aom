---
name: libaom/guard-vector-normalization-with-epsilon
description: When normalizing floating-point vectors or dividing by geometric norms, guard against zero lengths with an epsilon threshold check instead of assertions
rubric: correctness
severity: blocking
---

**Rule.** When normalizing floating-point vectors or dividing by geometric
norms, guard against zero or near-zero magnitudes inside the normalization
function using an epsilon tolerance check (such as `if (norm > eps)`). Do not
rely on caller-side input checks or assertions (like `assert(norm > 0)`) without
handling degenerate or near-collinear vectors gracefully.

**Why.** Floating-point rounding and precision limitations can cause computed
vectors (such as cross products of near-collinear vectors) to produce near-zero
magnitudes even when caller inputs appear valid. Asserting `norm > 0` and
dividing unconditionally leads to assertion failures or division by near-zero
(producing NaNs or infinities).

## Examples

Bad:

```cpp
static void normalize_carte_vector(CartesianVector *carte) {
  double temp =
      sqrt(carte->x * carte->x + carte->y * carte->y + carte->z * carte->z);
  // Brittle assertion triggers on near-zero norms from precision loss
  assert(temp > 0);
  carte->x = carte->x / temp;
  carte->y = carte->y / temp;
  carte->z = carte->z / temp;
}
```

Good:

```cpp
static void normalize_carte_vector(CartesianVector *carte) {
  double temp =
      sqrt(carte->x * carte->x + carte->y * carte->y + carte->z * carte->z);
  // Guard with an epsilon tolerance threshold before dividing
  if (temp > 0.0001) {
    carte->x = carte->x / temp;
    carte->y = carte->y / temp;
    carte->z = carte->z / temp;
  }
}
```
