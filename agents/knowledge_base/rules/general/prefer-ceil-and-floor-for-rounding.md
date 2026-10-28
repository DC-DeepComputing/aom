---
name: libaom/prefer-ceil-and-floor-for-rounding
description: When rounding floating-point expressions up or down to integers, use standard math functions like ceil() or floor() instead of ad-hoc formulas like round(0.5 + x)
rubric: readability
severity: nit
---

**Rule.** When rounding floating-point values up or down to integer boundaries,
use standard math functions (such as ceil(), floor(), round(), or trunc()) from
<math.h> or <cmath>. Do not use ad-hoc arithmetic formulas or offsets such as
round(0.5 + x) or (int)(x + 0.5) to emulate ceiling or floor rounding.

**Why.** Standard math library functions explicitly communicate mathematical
intent and handle floating-point edge cases correctly. Hand-rolled rounding
formulas like round(0.5 + x) introduce subtle arithmetic errors, such as
incorrectly incrementing exact integer values by 1.

## Examples

Bad:

```cpp
// Incorrectly increments exact integers (e.g., 1.0 -> round(1.5) -> 2)
rc->max_consec_drop = (int)round(
    0.5 + cpi->oxcf.rc_cfg.max_consec_drop_time * cpi->framerate / 1000);
```

Good:

```cpp
// Correctly rounds up to the nearest integer
rc->max_consec_drop = (int)ceil(cpi->oxcf.rc_cfg.max_consec_drop_time *
                                cpi->framerate / 1000);
```
