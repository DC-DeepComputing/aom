---
name: libaom/use-neon-multiply-accumulate-over-bitmask-add
description: When accumulating masked vector elements in ARM NEON routines, use multiply-accumulate intrinsics (vmlaq_*) with integer weights instead of separate bitmask AND and ADD operations
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON SIMD accumulation routines, use multiply-accumulate
intrinsics (such as vmlaq_u32, vmlaq_s32, or vmlaq_* / vmlal_*) with binary 0/1
coefficient masks or constant lane weights instead of separate bitmasking
(vandq_*) and addition (vaddq_*) steps.

**Why.** Using multiply-accumulate instructions (vmlaq_*) fuses coefficient
masking and addition into a single hardware operation per lane, reducing
instruction count, register pressure, and cycle overhead in performance-critical
accumulation and filtering loops.

## Examples

Bad:

```cpp
// Separate bitmask AND and vector addition
for (int j = 0; j < 5; j++) {
  const uint32x4_t vmask1 = valign(vzero, vmask, j % 4);
  const uint32x4_t vsrc1 = vandq_u32(vsrc[j][0], vmask1);
  vsum = vaddq_u32(vsum, vsrc1);
}
```

Good:

```cpp
// Fused multiply-accumulate with 0/1 mask
uint32x4_t vsums = vmulq_u32(vsrc[0][0], mask1);
vsums = vmlaq_u32(vsums, vsrc[1][0], mask1);
vsums = vmlaq_u32(vsums, vsrc[2][0], mask1);
vsums = vmlaq_u32(vsums, vsrc[3][0], mask1);
vsums = vmlaq_u32(vsums, vsrc[4][0], mask1);
```
