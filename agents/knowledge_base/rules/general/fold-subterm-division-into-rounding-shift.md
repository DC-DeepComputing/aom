---
name: libaom/fold-subterm-division-into-rounding-shift
description: When computing sums passed to ROUND_POWER_OF_TWO macros, fold intermediate sub-term divisions or right shifts into the final rounding shift parameter
rubric: safety
severity: should_fix
---

**Rule.** Do not perform intermediate integer divisions or right shifts on
individual sub-terms inside a sum passed to ROUND_POWER_OF_TWO,
ROUND_POWER_OF_TWO_SIGNED, or their 64-bit variants. Instead, scale the
remaining terms (e.g., left-shift by k bits) and incorporate the division into
the final rounding shift parameter (+ k) to preserve intermediate precision.

**Why.** Dividing or right-shifting individual terms before summing truncates
fractional bits prematurely and introduces redundant division operations.
Scaling the other terms up and increasing the final rounding shift folds the
division into a single rounding step at full intermediate precision.

## Examples

Bad:

```cpp
const int precision_value = WARPEDMODEL_PREC_BITS - SINE_PRECISION_BITS;
mi->wm_params.wmmat[0] =
    ROUND_POWER_OF_TWO_SIGNED((-mi_x * cosine_val) + (mi_y * sine_val) +
                                  (mi_x << SINE_PRECISION_BITS) +
                                  ((mv.col << SINE_PRECISION_BITS) / 8),
                              precision_value);
```

Good:

```cpp
const int precision_value = WARPEDMODEL_PREC_BITS - SINE_PRECISION_BITS + 3;
mi->wm_params.wmmat[0] =
    ROUND_POWER_OF_TWO_SIGNED((((-mi_x * cosine_val) + (mi_y * sine_val) +
                                (mi_x << SINE_PRECISION_BITS)) << 3) +
                                  (mv.col << SINE_PRECISION_BITS),
                              precision_value);
```
