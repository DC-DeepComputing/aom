---
name: libaom/eliminate-redundant-rounding-with-zero-shift
description: When shift or scale factors are constant zero, omit redundant rounding macros like ROUND_POWER_OF_TWO and intermediate array copies, accessing source values directly
rubric: readability
severity: nit
---

**Rule.** When power-of-two shifts, scale factors, or log scales are constant
zero, do not invoke rounding macros (such as ROUND_POWER_OF_TWO,
ROUND_POWER_OF_TWO_SIGNED, or ROUND_POWER_OF_TWO_64) or populate intermediate
array copies with the results. Access source pointers or scalar values directly
in subsequent calculations since rounding by zero is an identity operation.

**Why.** Invoking rounding macros with a shift of zero computes (x + 0) >> 0,
which is a no-op identity calculation. Retaining the rounding macro and storing
results in temporary intermediate arrays adds unnecessary stack storage, visual
clutter, and misleading code that suggests values are modified or scaled.

## Examples

Bad:

```cpp
const int log_scale = 0;
const int zbins[2] = { ROUND_POWER_OF_TWO(zbin_ptr[0], log_scale),
                       ROUND_POWER_OF_TWO(zbin_ptr[1], log_scale) };
for (int i = 0; i < 2; ++i) {
  thresh[i] = (zbins[i] * wt + prescan_add[i]) - 1;
}
```

Good:

```cpp
for (int i = 0; i < 2; ++i) {
  thresh[i] = (zbin_ptr[i] * wt + prescan_add[i]) - 1;
}
```
