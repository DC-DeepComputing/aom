---
name: libaom/prefer-static-inline-functions-over-function-macros
description: 'When implementing utility, arithmetic, calculation blocks, or SIMD helper routines, prefer static inline functions over function-like or multi-line #define macros'
rubric: readability
severity: should_fix
---

**Rule.** Define utility, arithmetic calculations, and SIMD helper routines as
static inline functions (using `static inline`, `static INLINE`, `static
AOM_INLINE`, or `static AOM_FORCE_INLINE`) rather than function-like or
multi-line `#define` macros. Even when helpers require multiple parameters or
out-pointers, pass explicit arguments instead of relying on macro variable
capture. Reserve preprocessor macros strictly for compile-time configuration,
stringification, or token-pasting operations.

**Why.** Static inline functions enforce strict compiler type checking, prevent
multiple evaluation of arguments with side effects, provide predictable local
variable scoping, and enable line-by-line debugger stepping, all while allowing
the compiler to optimize and inline just as effectively as preprocessor macros.

## Examples

Bad:

```cpp
// Bad: Single-line function macro lacks type safety and can double-evaluate arguments
#define pair_set_w16_epi16(a, b) \
  _mm256_set1_epi32((int32_t)(((uint16_t)(a)) | (((uint32_t)(b)) << 16)))

// Bad: Multi-line macro obscures variable scoping and hinders stepping in debuggers
#define CALC_AB_FAST_COMMON() \
  s0 = vmulq_u32(s0, const_n_val); \
  s1 = vmulq_u32(s1, const_n_val); \
  q0 = vmulq_u32(s4, s4); \
  q1 = vmulq_u32(s5, s5);
```

Good:

```cpp
// Good: Static inline function enforces types and evaluates parameters once
static inline __m256i pair_set_w16_epi16(int16_t a, int16_t b) {
  return _mm256_set1_epi32(
      (int32_t)(((uint16_t)(a)) | (((uint32_t)(uint16_t)(b)) << 16)));
}

// Good: Static inline helper with explicit parameter list and proper scoping
static INLINE void calc_ab_fast_common(
    uint32x4_t s0, uint32x4_t s1, uint32x4_t s4, uint32x4_t s5,
    uint32x4_t const_n_val, uint32x4_t *q0, uint32x4_t *q1) {
  s0 = vmulq_u32(s0, const_n_val);
  s1 = vmulq_u32(s1, const_n_val);
  *q0 = vmulq_u32(s4, s4);
  *q1 = vmulq_u32(s5, s5);
}
```
