---
name: libaom/aom_dsp/unify-simd-kernel-variants-with-nullable-pointer
description: When implementing SIMD or DSP kernel variants differing only by an optional operand, unify them into a single function using a nullable pointer parameter
rubric: design
severity: should_fix
---

**Rule.** Avoid duplicating near-identical SIMD or DSP kernel functions that
differ only by an optional operand or processing step (such as standard versus
averaged prediction). Unify the implementations into a single static inline
helper or template taking an optional pointer parameter defaulted to nullptr
(e.g., const uint8_t *second_pred = nullptr) and guarding the optional operation
with a null check that optimizing compilers eliminate for constant calls.

**Why.** Duplicating entire SIMD loop routines increases code size and
maintenance overhead across multiple architecture backends. When inlined or
called with constant arguments like nullptr, modern optimizing compilers perform
dead-code elimination to prune the conditional branch, generating optimal code
without duplicate function bodies.

## Examples

Bad:

```cpp
template <int BlockWidth>
HWY_MAYBE_UNUSED unsigned int SumOfAbsoluteDiff(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *ref_ptr,
                                                int ref_stride, int h) {
  // Duplicate loop computing SAD without second_pred...
}

template <int BlockWidth>
HWY_MAYBE_UNUSED unsigned int SumOfAbsoluteDiffAvg(const uint8_t *src_ptr,
                                                   int src_stride,
                                                   const uint8_t *ref_ptr,
                                                   int ref_stride, int h,
                                                   const uint8_t *second_pred) {
  // Duplicate loop computing SAD with second_pred averaging...
}
```

Good:

```cpp
template <int BlockWidth>
HWY_MAYBE_UNUSED unsigned int SumOfAbsoluteDiff(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, int h, const uint8_t *second_pred = nullptr) {
  // ...
  if (second_pred) {
    auto sec_pred_vec = hn::LoadU(pixel_tag, &second_pred[j]);
    auto ref2_vec = hn::AverageRound(ref_vec, sec_pred_vec);
    sad = hn::SumsOf8AbsDiff(src_vec, ref2_vec);
  } else {
    sad = hn::SumsOf8AbsDiff(src_vec, ref_vec);
  }
  // ...
}
```
