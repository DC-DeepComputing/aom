---
name: libaom/abbreviate-fraction-numerator-as-num-not-nom
description: When naming variables, parameters, or struct members representing fractions or scaling factors, abbreviate numerator as num or numerator, never nom
rubric: readability
severity: should_fix
---

**Rule.** When naming variables, function parameters, struct members, or macros
representing mathematical fractions or scaling factors, abbreviate numerator as
num (or spell out numerator) paired with denom or den. Do not use nom.

**Why.** In mathematical fractions and scaling ratios, 'numerator' is
universally abbreviated as 'num' (or spelled out) alongside 'denom' or 'den' for
denominator. Using 'nom' is non-standard, confusing (often arising from
mistakenly assuming 'nominator'), and inconsistent with libaom naming
conventions.

## Examples

Bad:

```cpp
void av1_calculate_scaled_superres_size(int *width, int *height,
                                        int superres_denom,
                                        int superres_nom);
```

Good:

```cpp
void av1_calculate_scaled_superres_size(int *width, int *height,
                                        int superres_denom,
                                        int superres_num);
```
