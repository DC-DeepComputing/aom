---
name: libaom/av1/use-standard-simd-transform-primitive-names
description: When writing SIMD transform primitives, use established subsystem naming conventions like btf_16_* for butterfly operations instead of novel ad-hoc names
rubric: readability
severity: should_fix
---

**Rule.** In SIMD and DSP transform implementations, adhere to established
subsystem naming conventions (such as btf_16_*, btf_32_*, or half_btf_* for
16-bit and 32-bit butterfly and scale operations) instead of introducing novel
or ad-hoc function names like inv_butterfly_*. Keep helper naming uniform across
architectures (ARM NEON, SSE2, SSSE3, AVX2) to facilitate cross-platform code
navigation.

**Why.** Consistent naming across architecture ports ensures transform
primitives are immediately recognizable across different SIMD backends, reducing
cognitive overhead and keeping DSP kernels maintainable.

## Examples

Bad:

```cpp
// Bad: introducing novel or ad-hoc naming for butterfly transform primitives
static INLINE void inv_butterfly_neon(const int16x8_t in0, int16_t coef1,
                                      int16_t coef2, int16x8_t *t0,
                                      int16x8_t *t1) {
  // ...
}
```

Good:

```cpp
// Good: using standard btf_16_* prefix consistent with other SIMD backends
static inline void btf_16_neon(const int16x8_t in0, int16_t coef1,
                               int16_t coef2, int16x8_t *t0,
                               int16x8_t *t1) {
  // ...
}
```
