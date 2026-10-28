---
name: libaom/aom_dsp/avoid-trivial-wrappers-around-single-simd-intrinsics
description: When implementing SIMD kernels, call standard intrinsics directly instead of creating trivial 1-to-1 wrapper functions that perform no type conversion or multi-step logic
rubric: design
severity: should_fix
---

**Rule.** Do not create trivial 1-to-1 wrapper functions around standard SIMD
intrinsics (such as wrapping vld1q_s16, vst1q_s16, or _mm_loadu_si128) when no
type conversion, multi-step sequence, or sub-vector layout adaptation is
performed. Call standard SIMD intrinsics directly at call sites unless an
operation encapsulates multi-instruction logic.

**Why.** Single-instruction wrapper functions introduce unnecessary abstraction
and indirection without providing encapsulation, safety, or data-conversion
benefits. Using standard intrinsics directly keeps SIMD kernel implementations
idiomatic and transparent to readers across architectures.

## Examples

Bad:

```cpp
// Trivial 1-to-1 wrapper around a single load intrinsic with no type conversion
static INLINE int16x8_t load_tran_lp_to_s16q(const tran_lp_t *coeff) {
  return vld1q_s16(coeff);
}

const int16x8_t src0 = load_tran_lp_to_s16q(coeff);
```

Good:

```cpp
// Call standard SIMD intrinsics directly when no multi-step logic is needed
const int16x8_t src0 = vld1q_s16(coeff);

// Reserve helper functions for multi-instruction sequences or conversions
static INLINE int16x8_t load_and_convert_s16q(const int32_t *coeff) {
  const int32x4_t lo = vld1q_s32(coeff);
  const int32x4_t hi = vld1q_s32(coeff + 4);
  return vcombine_s16(vmovn_s32(lo), vmovn_s32(hi));
}
```
