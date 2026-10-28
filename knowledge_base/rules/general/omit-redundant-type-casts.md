---
name: libaom/omit-redundant-type-casts
description: Omit redundant explicit type casts in assignments, function or SIMD intrinsic arguments, and expressions where types match or standard promotions apply
rubric: readability
severity: should_fix
---

**Rule.** Do not add redundant explicit type casts (such as `(int)`,
`(int32_t)`, `static_cast<int>`, `(float)`, `(double)`, or pointer casts) when
assigning variables, returning expressions, passing arguments to functions or
SIMD intrinsics (such as `_mm_set1_epi16` or `vdupq_n_u32`), or qualifying const
pointers where destination types already match or standard implicit conversions
safely apply. Additionally, rely on standard integer promotions and usual
arithmetic conversions: do not explicitly cast narrow integer types (`uint8_t`,
`int8_t`, `uint16_t`, `int16_t`) to `int` in arithmetic or bitwise expressions
(such as with `std::abs`), do not redundantly cast both operands in binary
operations (`+`, `-`, `*`, `/`) when widening one operand is sufficient, and
omit redundant floating-point casts on expressions already undergoing mixed
arithmetic promotion or helper return values (e.g., `round_shift()` or
`av1_compute_rd_mult()`).

**Why.** In C and C++, implicit conversions, pointer const-qualification,
integer promotion (promoting narrow integer types to `int`), and usual
arithmetic conversions automatically widen operands and convert matching types
without explicit casting. Redundant explicit casts add visual noise, clutter
code with unnecessary parentheses, create inconsistency across call sites, and
can mislead readers or mask subtle type mismatches when function signatures and
variables are refactored.

## Examples

Bad:

```cpp
// Redundant casts when passing arguments to SIMD intrinsics or functions:
__m128i scale = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
uint32x4_t vec = vdupq_n_u32((uint32_t)offset);

// Redundant casts when operand or destination types match:
if (tiles > (uint32_t)threads) {
  tiles = threads;
}
aom_roi_map_t *roi = (aom_roi_map_t *)data;
const uint8_t *next = (const uint8_t *)buf;
output[0] = (int32_t)round_shift(s0, bit);
rdmult = (int)av1_compute_rd_mult(base_qindex, bit_depth, ...);

// Redundant explicit casts on narrow integer types already promoted to int:
const int diff = (int)pixel_a - (int)pixel_b;
const int range = std::abs(static_cast<int>(opaque_val) - trans_val) + 1;

// Redundantly casting both operands in binary arithmetic or mixed float promotion:
sum = (int64_t)val1 + (int64_t)val2;
*rdmult = (int)((double)(*rdmult * cpi->sm_scaling_factor[idx]));
```

Good:

```cpp
// Pass expressions and matching variables directly to intrinsics and functions:
__m128i scale = _mm_set1_epi16(1 << SMOOTH_WEIGHT_LOG2_SCALE);
uint32x4_t vec = vdupq_n_u32(offset);

// Direct assignment, matching function returns, and implicit const qualification:
if (tiles > threads) {
  tiles = threads;
}
aom_roi_map_t *roi = data;
const uint8_t *next = buf;
output[0] = round_shift(s0, bit);
rdmult = av1_compute_rd_mult(base_qindex, bit_depth, ...);

// Rely on standard integer promotion of narrow integer types:
const int diff = pixel_a - pixel_b;
const int range = std::abs(opaque_val - trans_val) + 1;

// Widen only one operand; rely on implicit floating-point promotion:
sum = (int64_t)val1 + val2;
*rdmult = (int)(*rdmult * cpi->sm_scaling_factor[idx]);
```
