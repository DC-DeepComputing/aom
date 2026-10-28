---
name: libaom/avoid-redundant-neon-multiply-accumulate
description: In ARM NEON routines, avoid redundant multiply-accumulate intrinsics (vmla*/vmlaq*) for zero accumulator initialization or unit multipliers; use direct assignment, vmulq, or vaddq.
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON SIMD implementations, do not use vector
multiply-accumulate intrinsics (such as vmlaq_*, vmla_*, vmlal_*, or vmlaq_n_*)
when initializing an accumulator from zero or when multiplying by an all-ones
vector or scalar constant of 1. Initialize the accumulator directly with the
source vector or a multiplication intrinsic (vmulq_*), and use direct vector
addition (vaddq_*, vadd_*) rather than multiply-accumulate for terms with a unit
weight or identity multiplier.

**Why.** Multiplying by an all-ones vector or unit scalar constant is an
identity operation, and accumulating into zero is redundant. Initializing the
accumulator directly or using vector addition eliminates redundant
multiplication hardware instructions, reduces execution latency, lowers
execution port pressure, and frees vector registers previously dedicated to
holding constant zero or unit vectors.

## Examples

Bad:

```cpp
// Redundant multiply-accumulate with zero accumulator and identity multipliers
const uint32x4_t v_ones = vdupq_n_u32(1);
uint32x4_t vsum = vzero;
vsum = vmlaq_u32(vsum, vsrc[0][0], v_ones);
vsum = vmlaq_u32(vsum, vsrc[1][0], v_ones);
vsum = vmlaq_u32(vsum, vsrc[2][0], weights);
```

Good:

```cpp
// Direct initialization and vector addition without identity multiplications
uint32x4_t vsum = vsrc[0][0];
vsum = vaddq_u32(vsum, vsrc[1][0]);
vsum = vmlaq_u32(vsum, vsrc[2][0], weights);
```
