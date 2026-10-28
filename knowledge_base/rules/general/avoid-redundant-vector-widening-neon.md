---
name: libaom/avoid-redundant-vector-widening-neon
description: In ARM NEON routines, use direct widening (vaddl/vaddw/vsubw/vmlal) or pairwise accumulation (vpadal/vpadalq) instead of separate vmovl, vpaddl, or vaddq sequences
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON SIMD routines, use direct widening arithmetic and
accumulation intrinsics (such as vaddl_*, vsubl_*, vmull_*, vmlal_*, vaddw_*, or
vsubw_*) and direct pairwise add-accumulate intrinsics (vpadal_*, vpadalq_*)
rather than performing separate vector widening (such as vmovl_* or vpaddl_*
followed by vaddq_*/vsubq_*, or split vget_low_*/vget_high_* extractions with
vaddl_* and vaddq_*). Additionally, pass upper-half vector extractions
(vget_high_*) directly into widening intrinsics instead of delegating them to
helper functions taking 64-bit vector parameters, allowing compilers to emit
high-half widening instructions (such as SADDL2, SMULL2, or SMLAL2) rather than
redundant EXT and low-half widening instruction pairs.

**Why.** Explicit widening before arithmetic (vmovl_*), two-step widening
additions (vaddl_* or vpaddl_* followed by vaddq_*), and manual half-vector
extractions (vget_low_*/vget_high_* with vaddl_*) introduce unnecessary
intermediate registers, increase register pressure, and add pipeline latency
compared to single hardware instructions (vaddl_*, vaddw_*, vsubw_*, vmlal_*,
vpadal_*/vpadalq_*) that widen and accumulate in one operation. Similarly,
passing vget_high_* extractions to helper functions taking 64-bit parameters
hides the 128-bit vector context from the compiler, preventing the generation of
high-half widening instructions (such as SMLAL2, SADDL2, or SMULL2) and forcing
redundant vector extraction (EXT) instructions.

## Examples

Bad:

```cpp
// Bad: Redundant explicit widening before arithmetic
uint32x4_t p0 = vmovl_u16(d0);
p0 = vsubq_u32(sgrproj_sgr, p0);

// Bad: Widening narrow vectors individually with vaddl then adding with vaddq
const uint32x4_t tmp_lo = vaddl_u16(vget_low_u16(accum_lo), vget_low_u16(accum_hi));
const uint32x4_t tmp_hi = vaddl_u16(vget_high_u16(accum_lo), vget_high_u16(accum_hi));
accum32_lo = vaddq_u32(tmp_lo, accum32_lo);
accum32_hi = vaddq_u32(tmp_hi, accum32_hi);

// Bad: Pairwise widening into temporary or split halves followed by separate vaddq
const int64x2_t sum_l = vpaddlq_s32(dist_min);
sum = vaddq_s64(sum, sum_l);

// Bad: Passing high-half extraction to 64-bit helper prevents SMLAL2 emission
int16x4_t s0_hi = vget_high_s16(s0);
*sum1 = convolve_helper_64(s0_hi, y_filter, *sum1);
```

Good:

```cpp
// Good: Combined widening arithmetic in a single instruction
uint32x4_t p0 = vsubl_u16(sgrproj_sgr, d0);

// Good: Adding at narrow precision and accumulating directly using vaddw
const uint16x8_t sum = vaddq_u16(accum_lo, accum_hi);
accum32_lo = vaddw_u16(accum32_lo, vget_low_u16(sum));
accum32_hi = vaddw_u16(accum32_hi, vget_high_u16(sum));

// Good: Pairwise sum, widen to 64-bit, and accumulate directly in one instruction
sum = vpadalq_s32(sum, dist_min);

// Good: Passing vget_high_* directly enables SMLAL2 instruction generation
*sum1 = vmlal_lane_s16(*sum1, vget_high_s16(s0), y_filter_lo, 0);
```
