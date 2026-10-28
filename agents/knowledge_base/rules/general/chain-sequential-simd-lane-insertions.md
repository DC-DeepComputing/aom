---
name: libaom/chain-sequential-simd-lane-insertions
description: When performing sequential SIMD lane insertions on a vector, pass the returned vector from each insertion into subsequent calls to prevent overwriting earlier modifications
rubric: correctness
severity: should_fix
---

**Rule.** When modifying multiple lanes of a SIMD vector sequentially using
lane-insertion intrinsics (such as ARM Neon vset_lane_*/vsetq_lane_* or x86
_mm_insert_*/_mm256_insert_*), pass the vector returned from the preceding
insertion into each subsequent call. Do not pass an un-updated or different base
vector, which turns previous insertions into dead stores and corrupts vector
state.

**Why.** SIMD lane-insertion intrinsics return a new vector value rather than
modifying the vector in place. Passing the original un-updated vector or a
different operand to a subsequent insertion immediately overwrites prior lane
updates, dropping modifications and corrupting vector contents.

## Examples

Bad:

```cpp
// Incorrect: Second insertion uses vtmp2 or stale vtmp1, discarding lane 0 insertion
const uint32_t lane2 = vgetq_lane_u32(vtmp1, 2);
vtmp1 = vsetq_lane_u32(lane2, vtmp1, 0);
vtmp1 = vsetq_lane_u32(lane2, vtmp2, 1);
```

Good:

```cpp
// Correct: Sequential insertions chain the updated vector operand
const uint32_t lane2 = vgetq_lane_u32(vtmp1, 2);
vtmp1 = vsetq_lane_u32(lane2, vtmp1, 0);
vtmp1 = vsetq_lane_u32(lane2, vtmp1, 1);
```
