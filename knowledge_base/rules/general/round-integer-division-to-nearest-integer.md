---
name: libaom/round-integer-division-to-nearest-integer
description: When computing integer averages or performing integer division by constant divisors, add half the divisor or use ROUND_POWER_OF_TWO to round rather than truncate
rubric: correctness
severity: nit
---

**Rule.** When computing integer averages, scaling down, or performing integer
division by a constant divisor D, add half the divisor (D / 2) to the numerator
to round to the nearest integer instead of truncating toward zero. For
power-of-two divisions or right shifts, use ROUND_POWER_OF_TWO,
ROUND_POWER_OF_TWO_SIGNED, or 64-bit rounding macros from aom_ports/mem.h rather
than raw right-shift (>>) or bare division.

**Why.** Standard C/C++ integer division and bitwise right shifts truncate
positive quotients toward zero, introducing systematic downward bias in DSP
transforms, variance calculations, and filter averages. Adding half the divisor
prior to division rounds to the nearest integer, minimizing cumulative
truncation distortion.

## Examples

Bad:

```cpp
// Truncates toward zero, introducing downward bias
const unsigned int variance =
    (variances[lower_index] + (variances[middle_index] * 2) +
     variances[upper_index]) /
    4;
```

Good:

```cpp
// Adds half the divisor (+2 for divisor 4) to round to nearest integer
const unsigned int variance =
    (variances[lower_index] + (variances[middle_index] * 2) +
     variances[upper_index] + 2) /
    4;
```
