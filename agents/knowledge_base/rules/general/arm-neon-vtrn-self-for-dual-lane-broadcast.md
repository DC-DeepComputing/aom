---
name: libaom/arm-neon-vtrn-self-for-dual-lane-broadcast
description: When broadcasting both halves or lanes of an ARM NEON vector into separate vectors, transpose the vector with itself using vtrn instead of multiple vdup_lane calls
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON SIMD routines, when duplicating or broadcasting both
halves or lanes of a vector into separate vectors, use vtrn (such as vtrn_s32 or
vtrn_u32) to transpose the vector with itself. Do not issue multiple separate
vdup_lane_* calls to extract and duplicate each lane.

**Why.** Calling vtrn with identical vector inputs produces [lane0, lane0] and
[lane1, lane1] in a single hardware instruction (e.g., vtrn.32), whereas
multiple vdup_lane calls require separate instructions and increase register
pressure.

## Examples

Bad:

```cpp
// Duplicating both halves using multiple vdup_lane calls
int8x8_t ps0_s8 = vreinterpret_s8_s32(vdup_lane_s32(vreinterpret_s32_s8(pq_s0), 0));
int8x8_t qs0_s8 = vreinterpret_s8_s32(vdup_lane_s32(vreinterpret_s32_s8(pq_s0), 1));
```

Good:

```cpp
// Duplicating both halves in a single instruction via self-transposition
int32x2x2_t ps0_qs0 = vtrn_s32(vreinterpret_s32_s8(pq_s0), vreinterpret_s32_s8(pq_s0));
int8x8_t ps0_s8 = vreinterpret_s8_s32(ps0_qs0.val[0]);
int8x8_t qs0_s8 = vreinterpret_s8_s32(ps0_qs0.val[1]);
```
