---
name: libaom/arm-neon-combine-shift-and-narrow-with-vshrn
description: When right-shifting and narrowing ARM NEON vectors by compile-time constants, use immediate narrowing shifts (vshrn_n_*, vrshrn_n_*) instead of separate shift and narrow instructions
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON vector implementations, use immediate shift-and-narrow
intrinsics (such as vshrn_n_*, vrshrn_n_*, vqrshrn_n_*, vqshrn_n_*, or
vqshrun_n_*) when shifting right by compile-time constants and narrowing element
bit-width. Do not perform separate full-width shifts (vshrq_n_*, vrshrq_n_*) or
load constant shift amounts into vector registers for variable shifts (vrshlq_*,
vshlq_*) followed by separate narrowing or move-narrow instructions (vmovn_*,
vqmovn_*, vqmovun_*).

**Why.** Shift-right-and-narrow NEON instructions combine bit shifting
(including optional rounding or saturation) and lane narrowing into a single
hardware instruction using an immediate operand. Chaining separate shift and
move-narrow instructions, or loading constants into vector registers to call
variable shift instructions followed by narrow moves, increases instruction
count, wastes vector registers, and adds avoidable execution latency.

## Examples

Bad:

```cpp
// Chaining full-width shifts with narrow moves
const int32x4_t shifted_low = vshrq_n_s32(res_low, AOM_BLEND_A64_ROUND_BITS);
const int32x4_t shifted_high = vshrq_n_s32(res_high, AOM_BLEND_A64_ROUND_BITS);
int16x8_t res = vcombine_s16(vmovn_s32(shifted_low), vmovn_s32(shifted_high));

// Loading immediate constant into runtime vector for variable shift + narrowing
const int32x4_t shift = vdupq_n_s32(-DIST_PRECISION_BITS);
uint16x8_t avg = vcombine_u16(vmovn_u32(vrshlq_u32(cp0, shift)),
                              vmovn_u32(vrshlq_u32(cp1, shift)));
```

Good:

```cpp
// Combining right-shift and narrowing in a single intrinsic step
int16x8_t res = vcombine_s16(vshrn_n_s32(res_low, AOM_BLEND_A64_ROUND_BITS),
                             vshrn_n_s32(res_high, AOM_BLEND_A64_ROUND_BITS));

// Using immediate rounding narrowing shift directly
uint16x8_t avg = vcombine_u16(vrshrn_n_u32(cp0, DIST_PRECISION_BITS),
                              vrshrn_n_u32(cp1, DIST_PRECISION_BITS));
```
