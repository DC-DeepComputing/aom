---
name: libaom/av1/reflect-spherical-latitude-across-poles
description: When normalizing spherical coordinates across poles, reflect latitude phi across +/-PI/2 and shift longitude theta by PI instead of using modulo operations
rubric: correctness
severity: should_fix
---

**Rule.** When normalizing spherical coordinates (latitude phi and longitude
theta) into canonical ranges (phi in [-PI/2, PI/2]), reflect phi across the pole
(e.g., PI - phi or -PI - phi) and offset theta by PI when crossing +/-PI/2. Do
not use linear modulo operations like fmod(phi, 0.5 * PI), which fail to reflect
latitude and distort projection mappings across poles.

**Why.** Spherical coordinates do not wrap linearly across the poles; traversing
past +/-PI/2 latitude places the point on the opposite side of the sphere at an
inverted angle with longitude shifted by PI. A naive modulo operation wraps to
an incorrect latitude without flipping longitude, corrupting geometric
projections.

## Examples

Bad:

```cpp
// Modulo wrapping wraps latitude linearly without polar reflection or longitude flipping
double phi_mod = fmod(phi, 0.5 * PI);
if (phi_mod == 0 && fmod(phi, PI) != 0) {
  phi_mod = phi > 0 ? 0.5 * PI : -0.5 * PI;
}
```

Good:

```cpp
// Reflect latitude across the pole and offset longitude by PI when out of range
if (phi_mod < -0.5 * PI) {
  phi_mod = -PI - phi_mod;
  theta += PI;
} else if (phi_mod > 0.5 * PI) {
  phi_mod = PI - phi_mod;
  theta += PI;
}
```
