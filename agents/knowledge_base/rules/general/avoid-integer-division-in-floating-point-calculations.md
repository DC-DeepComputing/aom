---
name: libaom/avoid-integer-division-in-floating-point-calculations
description: When computing floating-point coordinates, angles, midpoints, or continuous formulas from integer dimensions, use floating-point arithmetic (* 0.5 or / 2.0) instead of truncating integer division
rubric: correctness
severity: should_fix
---

**Rule.** When calculating coordinates, angles, midpoints, fractional offsets,
or ratios in floating-point expressions involving integer variables (such as
dimensions, coordinates, or pixel offsets), use floating-point arithmetic (e.g.,
multiplying by 0.5 / 0.5f or dividing by 2.0 / 2.0f) or explicitly cast operands
to double or float before division. Do not use truncating integer division
(e.g., width / 2 or (left + right) / 2) in intermediate sub-expressions.

**Why.** In C and C++, dividing an integer variable by an integer literal (e.g.,
dim / 2) truncates fractional remainders toward zero before the result is
promoted to floating point. When operating on odd dimensions, fractional
coordinates, midpoints, or continuous geometric formulas (such as coordinate
transformations, spherical projections, and interpolation), this premature
truncation introduces systematic quantization distortion, alignment loss, and
off-by-half coordinate errors.

## Examples

Bad:

```cpp
// Truncating integer division discards fractional precision for odd dimensions/offsets
double phi = (y - height / 2) / (double)height * PI;
double center_x = (left + right) / 2;
*x = (theta + PI) / (2 * PI) * width;
double offset = (theta / PI) * (double)width / 2.0 + width / 2;
```

Good:

```cpp
// Floating-point multiplication and literals preserve full fractional precision
double phi = (y - height * 0.5) / (double)height * PI;
double center_x = (left + right) * 0.5;
*x = (theta + PI) / (2.0 * PI) * (double)width;
double offset = (theta / PI) * (double)width * 0.5 + width * 0.5;
```
