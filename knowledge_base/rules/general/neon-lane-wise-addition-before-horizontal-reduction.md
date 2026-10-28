---
name: libaom/neon-lane-wise-addition-before-horizontal-reduction
description: When accumulating vector elements in ARM NEON loops before a final horizontal reduction, use lane-wise addition (vaddq_*) instead of pairwise addition (vpaddq_*)
rubric: safety
severity: should_fix
---

**Rule.** When accumulating vector elements inside loops that perform a full
horizontal reduction at the end, use standard lane-wise addition (such as
vaddq_* or vadd_*) rather than pairwise addition (vpaddq_* or vpadd_*).
Intermediate lane order is irrelevant for full horizontal sums, and lane-wise
addition is faster and universally portable across AArch64 and AArch32.

**Why.** Pairwise addition adds adjacent vector elements, which incurs higher
instruction latency, extra cross-lane shuffling, and requires multi-instruction
emulation on 32-bit ARM. Because addition is associative and commutative over a
full horizontal reduction, standard lane-wise addition computes the exact same
total sum with lower cycle counts and without architecture-specific conditional
workarounds.

## Examples

Bad:

```cpp
for (int j = 0; j < 5; j++) {
  // Unnecessary pairwise addition for intermediate accumulation.
#if AOM_ARCH_AARCH64
  vsum = vpaddq_u32(vsum, vsrc1);
  vsum = vpaddq_u32(vsum, vsrc2);
#else
  const uint32x4_t vtmp = vaddq_u32(vsrc1, vsrc2);
  const uint32x2_t lo = vpadd_u32(vget_low_u32(vsum), vget_high_u32(vsum));
  const uint32x2_t hi = vpadd_u32(vget_low_u32(vtmp), vget_high_u32(vtmp));
  vsum = vcombine_u32(lo, hi);
#endif
}
acc[row][col] = horizontal_add_u32x4(vsum);
```

Good:

```cpp
for (int j = 0; j < 5; j++) {
  // Use lane-wise addition; order across lanes does not matter before reduction.
  vsum = vaddq_u32(vsum, vsrc1);
  vsum = vaddq_u32(vsum, vsrc2);
}
acc[row][col] = horizontal_add_u32x4(vsum);
```
