---
name: libaom/avoid-premature-widening-in-simd-accumulation-loops
description: When accumulating differences or products in SIMD loops, do not prematurely widen and reset intermediate accumulators midway through rows
rubric: safety
severity: should_fix
---

**Rule.** In SIMD accumulation loops (such as NEON or SSE/AVX SAD, SSD, or
variance calculations), do not perform premature intermediate widening or
accumulator resets when the narrower vector type has sufficient bit-width
headroom to safely accumulate across the entire loop chunk or row. Accumulate
intermediate values in narrower vector registers for as many iterations as
mathematically safe before widening to wider accumulators.

**Why.** Narrower SIMD vectors process twice as many elements per register and
reduce register pressure compared to wider types. Intermediate 16-bit
accumulators have substantial dynamic range headroom (e.g., accumulating up to
257 8-bit difference terms without exceeding 65,535 in uint16), so deferring
widening to the end of a row or loop eliminates redundant widening instructions
and register zeroing.

## Examples

Bad:

```cpp
// Premature widening to uint32 and accumulator reset midway through a 128-byte row:
vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_48), vget_low_u8(vec_ref_48));
vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_48), vget_high_u8(vec_ref_48));
tmp_lo = vaddl_u16(vget_low_u16(vec_accum_lo), vget_low_u16(vec_accum_hi));
tmp_hi = vaddl_u16(vget_high_u16(vec_accum_lo), vget_high_u16(vec_accum_hi));
vec_accum_32lo = vaddq_u32(tmp_lo, vec_accum_32lo);
vec_accum_32hi = vaddq_u32(tmp_hi, vec_accum_32hi);

vec_accum_lo = vdupq_n_u16(0);
vec_accum_hi = vdupq_n_u16(0);
```

Good:

```cpp
// Accumulate all chunks across the row in uint16, then widen once at the end of the row:
vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_112), vget_low_u8(vec_ref_112));
vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_112), vget_high_u8(vec_ref_112));

const uint16x8_t tmp = vaddq_u16(vec_accum_lo, vec_accum_hi);
vec_accum_32lo = vaddw_u16(vec_accum_32lo, vget_low_u16(tmp));
vec_accum_32hi = vaddw_u16(vec_accum_32hi, vget_high_u16(tmp));
```
