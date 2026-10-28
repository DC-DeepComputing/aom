---
name: libaom/arm-neon-initialize-lane0-with-vdup-n
description: When constructing an ARM NEON vector lane-by-lane from scalars, initialize with vdup[q]_n_* using the first scalar instead of zero-initializing and calling vset_lane_* for lane 0
rubric: safety
severity: should_fix
---

**Rule.** When constructing an ARM NEON vector lane-by-lane from scalar values,
initialize the vector using `vdup_n_*` (or `vdupq_n_*`) with the first scalar
value. Do not zero-initialize the vector with `vdup_n_*(0)` and then insert lane
0 with `vset_lane_*(val, vec, 0)`.

**Why.** Initializing the vector directly with `vdup_n_*(val)` populates lane 0
upon creation, eliminating the extra instructions of creating a zero vector and
performing an initial `vset_lane_*` insertion for lane 0.

## Examples

Bad:

```cpp
// Redundant zero-initialization and lane 0 insertion
uint32x2_t a_u32 = vdup_n_u32(0);
a_u32 = vset_lane_u32(val0, a_u32, 0);
a_u32 = vset_lane_u32(val1, a_u32, 1);
```

Good:

```cpp
// Populate lane 0 directly during vector initialization
uint32x2_t a_u32 = vdup_n_u32(val0);
a_u32 = vset_lane_u32(val1, a_u32, 1);
```
