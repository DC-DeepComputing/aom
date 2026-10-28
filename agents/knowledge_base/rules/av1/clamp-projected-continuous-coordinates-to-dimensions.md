---
name: libaom/av1/clamp-projected-continuous-coordinates-to-dimensions
description: When implementing coordinate projections and transformations, clamp computed output coordinates using AOMMIN or fclamp instead of asserting against input boundary angles
rubric: correctness
severity: should_fix
---

**Rule.** When implementing coordinate transformation or projection utilities
(such as spherical-to-planar, polar, or equirectangular mappings), do not impose
artificial input domain constraints or assertions on boundary angles (such as
poles at phi = +/- pi/2) and do not use exact floating-point assertions to guard
against boundary values. Allow continuous floating-point evaluation and handle
mathematical singularities by regulating or clamping output coordinates strictly
within valid buffer bounds [0.0, dimension - eps] using AOMMIN, AOMMAX, clamp,
or fclamp instead of shifting boundary validation onto callers.

**Why.** Valid boundary inputs (such as pole latitudes) naturally evaluate to
exact dimension boundaries (e.g., y = height). Because 0-indexed raster sampling
and interpolation require continuous coordinates strictly in [0, dimension),
unconstrained evaluations or exact floating-point boundary assertions cause
brittle assertion crashes, numeric rounding failures, and out-of-bounds buffer
sampling. Clamping computed coordinates internally ensures mathematical
robustness across all continuous inputs without forcing callers to implement
bespoke boundary guards.

## Examples

Bad:

```cpp
// Variant 1: Exact floating-point boundary assertion on pole angles
void sphere_to_plane_erp(double phi, double theta, int width, int height,
                         double *x, double *y) {
  assert(fmod(phi, PI) != 0.5 * PI);
  *x = theta / (2 * PI) * width + width * 0.5;
  *y = phi / (PI * 0.5) * height * 0.5 + height * 0.5;
}

// Variant 2: Shifting boundary guards to callers via input assertions
void plane_to_sphere_erp(double x, double y, int width, int height,
                         double *phi, double *theta) {
  assert(fabs(y - height / 2.0) < height / 2.0);
  *phi = (y / height - 0.5) * PI;
}
```

Good:

```cpp
// Variant 1: Continuous evaluation with output clamping to [0, dimension)
void sphere_to_plane_erp(double phi, double theta, int width, int height,
                         double *x, double *y) {
  *x = theta / (2 * PI) * width + width * 0.5;
  *y = phi / (PI * 0.5) * height * 0.5 + height * 0.5;
  *x = fclamp(*x, 0.0, (double)width - 0.001);
  *y = AOMMIN(*y, (double)height - 0.001);
}

// Variant 2: Clamping internal coordinates without artificial input restrictions
void plane_to_sphere_erp(double x, double y, int width, int height,
                         double *phi, double *theta) {
  y = AOMMIN(AOMMAX(y, 0.0), (double)height - 0.001);
  *phi = (y / height - 0.5) * PI;
}
```
