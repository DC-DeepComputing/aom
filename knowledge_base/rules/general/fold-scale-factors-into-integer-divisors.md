---
name: libaom/fold-scale-factors-into-integer-divisors
description: When scaling integer expressions with constant divisors, fold multipliers into denominators as x / (D / S) instead of (x / D) * S to avoid precision truncation
rubric: correctness
severity: should_fix
---

**Rule.** When scaling integer expressions that involve constant or known
divisors, fold integer multipliers into the denominator as 'x / (D / S)' rather
than computing '(x / D) * S' (provided 'D' is evenly divisible by 'S'). Avoid
performing integer division prior to multiplying by a scale factor, which
truncates the fractional bits of the intermediate quotient and forces the least
significant bits of the scaled result to zero.

**Why.** In integer arithmetic, evaluating division before multiplication
discards the remainder at the division step, truncating precision and causing
the least significant bits of the final multiplied value to be zero. Folding the
multiplier into the divisor or multiplying before dividing preserves full
arithmetic precision.

## Examples

Bad:

```cpp
// Truncates lower bits during intermediate integer division by D
int64_t scaled = (x / D) * S;

// Complex expression truncating precision before multiplying by scale factors
val += term * a[k] / (scale * SCALE_FACTOR) * (a[l] / scale_l) / SCALE_FACTOR * (scale_l * scale);
```

Good:

```cpp
// Preserves precision by folding scale factor S into divisor D (when D % S == 0)
int64_t scaled = x / (D / S);

// Folded scale factor prevents intermediate precision loss
val += term * a[k] / (scale * SCALE_FACTOR) * a[l] / (SCALE_FACTOR / scale);
```
