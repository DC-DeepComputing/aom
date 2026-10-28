---
name: libaom/combine-neon-vmul-and-vadd-into-vmla
description: When computing vector products and additions in ARM NEON code, fuse separate vmul and vadd into vmla by using the addend as accumulator
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON SIMD implementations, combine separate vector
multiplication (vmul_*, vmulq_*) and vector addition (vadd_*, vaddq_*) into
multiply-accumulate intrinsics (vmla_*, vmlaq_*, or vmlal_*). When accumulating
a combination of multiplied terms and an unmultiplied vector, pass the
unmultiplied addend as the initial accumulator operand to the first
multiply-accumulate operation instead of executing a trailing vector add.

**Why.** Using fused multiply-accumulate instructions (vmla/vmlaq/vmlal)
eliminates standalone addition instructions, reduces vector register pressure,
and minimizes execution latency in performance-critical SIMD routines.

## Examples

Bad:

```cpp
// Separate vmulq and trailing vaddq
uint32x4_t vsum = vmulq_u32(vsrc[0], weights[0]);
vsum = vmlaq_u32(vsum, vsrc[1], weights[1]);
vsum = vmlaq_u32(vsum, vsrc[2], weights[2]);
vsum = vaddq_u32(vsum, vsrc[3]);
```

Good:

```cpp
// Fused vmlaq using the unmultiplied addend as initial accumulator
uint32x4_t vsum = vmlaq_u32(vsrc[3], vsrc[0], weights[0]);
vsum = vmlaq_u32(vsum, vsrc[1], weights[1]);
vsum = vmlaq_u32(vsum, vsrc[2], weights[2]);
```
