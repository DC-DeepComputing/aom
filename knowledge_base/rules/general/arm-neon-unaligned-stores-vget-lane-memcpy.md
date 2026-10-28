---
name: libaom/arm-neon-unaligned-stores-vget-lane-memcpy
description: When implementing unaligned ARM NEON lane stores, extract the scalar with vget_lane_* and use small-immediate memcpy instead of vst1_lane_* with pointer casts
rubric: correctness
severity: should_fix
---

**Rule.** In ARM NEON memory helper functions and kernels, perform unaligned or
narrow-aligned sub-vector stores by extracting the scalar lane with vget_lane_*
and storing via small fixed-size memcpy (e.g., 2, 4, or 8 bytes). Do not cast
destination pointers to wider integer types and write with vst1_lane_*.

**Why.** Casting potentially unaligned or narrower-aligned pointers (such as
uint16_t * or uint8_t *) to wider types (such as uint32_t * or uint64_t *) for
vst1_lane_* intrinsics assumes strict natural alignment, risking alignment
faults or undefined behavior on 32-bit ARM (ARMv7). Extracting lanes with
vget_lane_* and copying via fixed-size memcpy is well-defined for arbitrary
alignment and compiles down to optimal unaligned store instructions in modern
compilers.

## Examples

Bad:

```cpp
// Bad: Casting uint16_t* to uint32_t* assumes 4-byte alignment and risks ARMv7 faults
static INLINE void store_u16_2x1(uint16_t *s, const uint16x4_t s0) {
  vst1_lane_u32((uint32_t *)s, vreinterpret_u32_u16(s0), 0);
}
```

Good:

```cpp
// Good: Extract lane to local scalar and store with small fixed-size memcpy
#define store_u16_2x1_lane(dst, src, lane)                       \
  do {                                                           \
    uint32_t a = vget_lane_u32(vreinterpret_u32_u16(src), lane); \
    memcpy(dst, &a, sizeof(a));                                  \
  } while (0)

static INLINE void store_u16_2x1(uint16_t *s, const uint16x4_t s0) {
  store_u16_2x1_lane(s, s0, 0);
}
```
