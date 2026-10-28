---
name: libaom/av1/use-floating-point-for-projection-coordinates
description: When declaring geometric or projection coordinate transformation functions, use floating-point types (double or float) rather than integers for coordinate parameters to preserve sub-pixel accuracy
rubric: correctness
severity: should_fix
---

**Rule.** When defining or declaring geometric projection and coordinate
transformation functions (such as spherical-to-planar, planar-to-spherical, or
cartesian-to-polar mappings), use floating-point types (double or float) for
spatial coordinate input and output parameters rather than integer types (int).
Ensure coordinate inputs retain continuous fractional positions to maintain
sub-pixel precision and round-trip symmetry across forward and inverse
transformations.

**Why.** Projection mappings model continuous mathematical domains where
sampling positions frequently lie on sub-pixel fractional coordinates.
Restricting input coordinates to integer types truncates fractional offsets,
destroys sub-pixel accuracy, and prevents seamless invertibility between forward
and inverse projection functions.

## Examples

Bad:

```cpp
// Truncates spatial coordinates to integer pixels, losing sub-pixel accuracy
void plane_to_equ(int x, int y, int width, int height, double *phi, double *theta);
```

Good:

```cpp
// Uses floating-point coordinates for continuous mapping and sub-pixel precision
void av1_plane_to_sphere_erp(double x, double y, int width, int height, double *phi, double *theta);
```
