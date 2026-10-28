---
name: libaom/initialize-neon-vectors-before-lane-loads
description: When passing vector variables to ARM NEON lane-inserting load intrinsics or helpers, initialize them first to prevent armv7 compiler warnings
rubric: correctness
severity: should_fix
---

**Rule.** Initialize ARM NEON vector variables (for example, using vdup_n_*,
vdupq_n_*, or vld1_dup_* for lane 0) before passing them to lane-inserting load
intrinsics (such as vld1_lane_* or vld1q_lane_*) or multi-lane load helpers
(load_u8_4x1, load_u16_2x1, load_u16_2x2). Do not leave vector variables
uninitialized when passing them as in-out operands to lane load operations.

**Why.** ARM NEON lane-loading intrinsics like vld1_lane_* take the destination
vector as an input argument to insert the loaded value into a specific lane. On
32-bit ARM (armv7-a), GCC does not recognize that only uninitialized lanes are
read and emits -Wmaybe-uninitialized warnings, breaking -Werror builds.

## Examples

Bad:

```cpp
uint16x4_t r;
load_u16_2x1(ref + 0 * ref_stride, &r, 0);
load_u16_2x1(ref + 1 * ref_stride, &r, 1);
```

Good:

```cpp
uint16x4_t r = vdup_n_u16(0);
load_u16_2x1(ref + 0 * ref_stride, &r, 0);
load_u16_2x1(ref + 1 * ref_stride, &r, 1);
```
