---
name: libaom/arm-neon-aarch64-horizontal-reduction-intrinsics
description: When performing horizontal reductions across ARM NEON vectors to scalar values, use sum_neon.h helpers or AArch64 reduction intrinsics instead of scalar lane extraction or broadcast sequences
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON SIMD implementations, perform horizontal reductions (sum,
min, or max across vector lanes into a scalar) using shared reduction helpers
from aom_dsp/arm/sum_neon.h (such as horizontal_add_* or horizontal_long_add_*)
or dedicated AArch64 reduction intrinsics (such as vaddv_*, vaddlv_*, vminv_*,
vmaxv_*) guarded by #if AOM_ARCH_AARCH64. Do not extract individual vector lanes
into general-purpose scalar registers with vgetq_lane_* or vget_lane_* to
compute scalar sums with arithmetic operators (+), do not use multi-step
pairwise additions (vpaddl_*/vadd_*) across vector registers when reduction
helpers or intrinsics exist, and do not use horizontal add-and-broadcast helpers
(horizontal_add_and_broadcast_*) when only a scalar result is needed.

**Why.** Extracting multiple vector lanes into general-purpose registers incurs
cross-register-bank transfer penalties and extra scalar ALU operations.
Furthermore, dedicated AArch64 hardware reduction instructions (vaddv, vaddlv,
vmaxv, vminv) reduce vector lanes directly to scalars in a single instruction
without multi-step pairwise latency. Using sum_neon.h helpers encapsulates
optimal architecture-specific paths (AArch64 vs AArch32) and avoids redundant
vector duplication incurred by add-and-broadcast sequences.

## Examples

Bad:

```cpp
// Anti-pattern 1: Extracting individual lanes into scalar registers to sum
int64_t sum1 = vgetq_lane_s64(r, 0) + vgetq_lane_s64(r, 1);

// Anti-pattern 2: Manual multi-step pairwise reduction without AArch64 intrinsics
uint16x4_t b = vpaddl_u8(a);
uint32x2_t c = vpaddl_u16(b);
int sum2 = vget_lane_u32(c, 0) + vget_lane_u32(c, 1);

// Anti-pattern 3: Using add-and-broadcast helper for a scalar sum
int sum3 = vgetq_lane_u32(horizontal_add_and_broadcast_long_u16x8(vec), 0);
```

Good:

```cpp
// Variant 1: Shared reduction helper from sum_neon.h
int64_t sum1 = horizontal_add_s64x2(r);
int sum3 = horizontal_add_u16x8(vec);

// Variant 2: Guarded AArch64 direct reduction intrinsic with AArch32 fallback
#if AOM_ARCH_AARCH64
int sum2 = vaddlv_u8(a);
#else
uint16x4_t b = vpaddl_u8(a);
uint32x2_t c = vpaddl_u16(b);
int sum2 = vget_lane_u32(c, 0) + vget_lane_u32(c, 1);
#endif
```
