---
name: libaom/avoid-right-shift-for-signed-downscaling
description: When downscaling or subsampling signed coordinates, motion vectors, or offsets, use signed division or signed rounding macros rather than arithmetic right shift
rubric: correctness
severity: should_fix
---

**Rule.** Do not use right shift (`>>`) to downscale or subsample signed values
(such as motion vectors, coordinates, deltas, or offsets) when rounding towards
zero or symmetric rounding is expected. Use signed integer division (`val / (1
<< shift)`) or signed rounding helper macros (such as
`ROUND_POWER_OF_TWO_SIGNED` or `ROUND_POWER_OF_TWO_SIGNED_64`) to prevent
asymmetric rounding between positive and negative numbers.

**Why.** In C/C++, arithmetic right shift on negative signed integers rounds
towards negative infinity (e.g., `-1 >> 1 == -1`), whereas integer division
truncates towards zero (e.g., `-1 / 2 == 0`). Using right shift on signed
quantities produces asymmetric downscaling between positive and negative values,
distorting symmetric coordinate, offset, or motion vector operations.

## Examples

Bad:

```cpp
// Right shift rounds negative values toward -infinity (-1 >> 1 == -1), introducing asymmetry.
clamped_mv.row = (int16_t)(src_mv->row >> ss_y);
clamped_mv.col = (int16_t)(src_mv->col >> ss_x);
```

Good:

```cpp
// Integer division truncates symmetrically toward zero for positive and negative values.
clamped_mv.row = (int16_t)(src_mv->row / (1 << ss_y));
clamped_mv.col = (int16_t)(src_mv->col / (1 << ss_x));

// Or use ROUND_POWER_OF_TWO_SIGNED for symmetric nearest-integer rounding:
// clamped_mv.row = (int16_t)ROUND_POWER_OF_TWO_SIGNED(src_mv->row, ss_y);
```
