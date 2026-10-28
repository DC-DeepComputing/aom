---
name: libaom/av1/use-fixed-point-tables-instead-of-floating-point-math
description: When implementing codec, transform, or motion compensation calculations, use precomputed fixed-point integer lookup tables and integer arithmetic instead of runtime floating-point trigonometry
rubric: safety
severity: should_fix
---

**Rule.** Do not use runtime floating-point calculations or library
trigonometric functions (such as sin(), cos(), or tan()) in core video coding,
motion compensation, or transform routines. Instead, precompute integer lookup
tables scaled to the required fixed-point precision (such as cospi tables) and
compute parameters using fixed-point integer arithmetic.

**Why.** Runtime floating-point math and trigonometric library calls introduce
performance overhead, hinder SIMD vectorization, and cause platform-dependent
rounding differences across target architectures.

## Examples

Bad:

```cpp
// In motion compensation or matrix setup:
double rotation_radians = (rotation * 0.2) * (PI / 180.0);
int32_t wmmat[8] = {
  (int32_t)16 * ((-mi_x) * cos(rotation_radians) +
                 mi_y * sin(rotation_radians) + mi_x + (mv.col / 8)),
  (int32_t)16 * ((-mi_x) * sin(rotation_radians) -
                 mi_y * cos(rotation_radians) + mi_y + (mv.row / 8)),
};
```

Good:

```cpp
// Use precomputed integer lookup tables and fixed-point arithmetic:
const int cos_val = av1_cos_table[rotation_idx];
const int sin_val = av1_sin_table[rotation_idx];
int32_t wmmat[8] = {
  (int32_t)(((-mi_x * cos_val + mi_y * sin_val) >> SINE_PRECISION_BITS) +
            mi_x + (mv.col / 8)),
  (int32_t)(((-mi_x * sin_val - mi_y * cos_val) >> SINE_PRECISION_BITS) +
            mi_y + (mv.row / 8)),
};
```
