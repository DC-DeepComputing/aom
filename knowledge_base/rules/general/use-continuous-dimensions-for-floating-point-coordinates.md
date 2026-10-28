---
name: libaom/use-continuous-dimensions-for-floating-point-coordinates
description: When asserting bounds, centering, or scaling continuous floating-point coordinates in projections, use full dimensions (dim) rather than discrete bounds (dim - 1)
rubric: correctness
severity: should_fix
---

**Rule.** When asserting bounds, computing coordinate centers, or scaling and
normalizing continuous floating-point coordinates (such as in spherical
projections, equirectangular mapping, or geometric transformations), use the
full continuous dimension (dim, such as width or height). Do not subtract 1 from
dimensions (e.g., width - 1 or height - 1) in floating-point ranges or
continuous coordinate transformation formulas.

**Why.** In continuous coordinate spaces, dimensions represent physical
continuous spans [0.0, dim]. Subtracting 1 (dim - 1) conflates continuous
coordinate spans with the maximum discrete integer index of a 0-indexed raster
array. Using dim - 1 artificially truncates the valid floating-point domain,
introduces off-by-half centering errors (e.g., (dim - 1) * 0.5 instead of dim *
0.5), and skews geometric scaling ratios in projections.

## Examples

Bad:

```cpp
// Variant 1: Continuous angular scaling skewed by subtracting 1
*x = theta_mod / PI * (width - 1) * 0.5 + (width - 1) * 0.5;
*y = phi_mod / (PI * 0.5) * (height - 1) * 0.5 + (height - 1) * 0.5;

// Variant 2: Bounds assertion and centering improperly using discrete index
assert(y >= 0.0 && y <= height - 1);
*phi = (y - (height - 1) * 0.5) / (height - 1) * PI;
```

Good:

```cpp
// Variant 1: Uses full dimensions for continuous angular mapping
*x = theta_mod / PI * width * 0.5 + width * 0.5;
*y = phi_mod / (PI * 0.5) * height * 0.5 + height * 0.5;

// Variant 2: Continuous assertion and centering using full dimension
assert(y >= 0.0 && y <= height);
*phi = (y - height * 0.5) / height * PI;
```
