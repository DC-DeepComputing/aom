---
name: libaom/avoid-unnecessary-widening-in-8bit-simd
description: When implementing 8-bit SIMD operations or comparisons against 16-bit upper bounds, operate directly at 8-bit width or saturating-narrow bounds rather than widening 8-bit vectors
rubric: safety
severity: should_fix
---

**Rule.** In SIMD (ARM NEON or x86) optimizations, perform arithmetic, absolute
differences, and threshold comparisons directly on 8-bit integer vectors when
values fit within 8 bits. Additionally, when comparing an unsigned 8-bit vector
against a 16-bit upper bound (x <= y), narrow the 16-bit vector using saturating
narrowing (vqmovn_u16 or _mm_packus_epi16) and compare at 8-bit precision
(vcleq_u8) rather than widening the 8-bit vector to 16 bits (vmovl_u8 or unpack
intrinsics).

**Why.** Widening 8-bit vectors to 16 bits splits data across multiple
registers, halving lane throughput and requiring extra unpacking and narrowing
instructions. Operating directly on 8-bit vectors or saturating 16-bit upper
bounds preserves comparison semantics while keeping computations within a single
8-bit vector instruction.

## Examples

Bad:

```cpp
// Bad: Unnecessarily widening 8-bit vectors to 16-bit for differences or upper-bound comparisons
const int16x8_t thresh_16x8 = vdupq_n_s16(thresh);
int16x8_t p4q4_s16 = vreinterpretq_s16_u16(vmovl_u8(*p4q4));
int16x8_t p0q0_s16 = vreinterpretq_s16_u16(vmovl_u8(*p0q0));
uint16x8_t flat_16x8 = vcgtq_s16(vabdq_s16(p4q4_s16, p0q0_s16), thresh_16x8);

const uint8x8_t x_le_top_left_low =
    vmovn_u16(vcleq_u16(vmovl_u8(vget_low_u8(x_dist)), top_left_dist_low));
const uint8x8_t x_le_top_left_high =
    vmovn_u16(vcleq_u16(vmovl_u8(vget_high_u8(x_dist)), top_left_dist_high));
return vcombine_u8(x_le_top_left_low, x_le_top_left_high);
```

Good:

```cpp
// Good: Operating directly on 8-bit vectors or saturating 16-bit upper bounds to 8-bit
const uint8x8_t thresh_8x8 = vdup_n_u8(thresh);
uint8x8_t flat_8x8 = vcgt_u8(vabd_u8(*p4q4, *p0q0), thresh_8x8);

const uint8x16_t top_left_dist = vcombine_u8(vqmovn_u16(top_left_dist_low),
                                             vqmovn_u16(top_left_dist_high));
return vcleq_u8(x_dist, top_left_dist);
```
