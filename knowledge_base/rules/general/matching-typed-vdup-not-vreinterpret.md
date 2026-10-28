---
name: libaom/matching-typed-vdup-not-vreinterpret
description: When broadcasting a scalar into an ARM NEON vector, duplicate directly with vdup[q]_n_<type> for the target type instead of chaining vmovl widening or vreinterpret casts
rubric: safety
severity: should_fix
---

**Rule.** When broadcasting or duplicating a scalar value into an ARM NEON
vector, use the `vdup_n_<type>` or `vdupq_n_<type>` intrinsic matching the
target vector type (casting the scalar argument if needed). Do not duplicate
into a narrower or different element type and chain `vmovl_*` widening or
`vreinterpret[q]_*` casts.

**Why.** Duplicating directly into the target vector type with a scalar cast
eliminates redundant vector widening instructions (such as `vmovl`), avoids
unnecessary vector reinterpret casts, and produces cleaner, more efficient NEON
code.

## Examples

Bad:

```cpp
// Redundant vector reinterpret or widening on duplicated scalar
const int16x4_t blimit_16x4 = vreinterpret_s16_u16(vdup_n_u16(*blimit));
const uint32x4_t mask_32x4 = vreinterpretq_u32_s32(vdupq_n_s32(val));
const int16x8_t limit_16x8 = vreinterpretq_s16_u16(vmovl_u8(vdup_n_u8(*limit)));
```

Good:

```cpp
// Duplicate directly into the target vector type with scalar cast if needed
const int16x4_t blimit_16x4 = vdup_n_s16((int16_t)*blimit);
const uint32x4_t mask_32x4 = vdupq_n_u32((uint32_t)val);
const int16x8_t limit_16x8 = vdupq_n_s16((int16_t)*limit);
```
