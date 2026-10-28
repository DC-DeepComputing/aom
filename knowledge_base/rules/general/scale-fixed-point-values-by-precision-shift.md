---
name: libaom/scale-fixed-point-values-by-precision-shift
description: When converting floating-point or fractional values to fixed-point integers using bit-precision constants, scale by (1 << PREC_BITS) rather than multiplying by the bit count directly
rubric: correctness
severity: should_fix
---

**Rule.** When converting floating-point values or fractional coordinates into
fixed-point integer representations defined by a bit-precision constant (e.g.,
`*_PREC_BITS`, `*_FRAC_BITS`, or `*_PRECISION_BITS`), scale the value by `(1 <<
PREC_BITS)`. Do not multiply or divide directly by the bit-precision constant
value ($N$) itself.

**Why.** A precision constant of $N$ bits specifies a fractional resolution of
$1 / 2^N$. Multiplying by the bit count $N$ instead of the power-of-two scaling
factor $(1 \ll N)$ under-scales the value by a factor of $2^N / N$ (e.g., 65536
/ 16 = 4096x), resulting in severe calculation and motion-vector scaling bugs.

## Examples

Bad:

```cpp
// Incorrect: multiplies by bit count 16 instead of (1 << 16)
int32_t wmmat[8] = {
  (int32_t)(16 * cos(rotation_radians)),
  (int32_t)(-16 * sin(rotation_radians)),
  0,
  0,
};
```

Good:

```cpp
// Correct: scales by (1 << WARPEDMODEL_PREC_BITS)
int32_t wmmat[8] = {
  (int32_t)round((1 << WARPEDMODEL_PREC_BITS) * cos(rotation_radians)),
  (int32_t)round(-(1 << WARPEDMODEL_PREC_BITS) * sin(rotation_radians)),
  0,
  0,
};
```
