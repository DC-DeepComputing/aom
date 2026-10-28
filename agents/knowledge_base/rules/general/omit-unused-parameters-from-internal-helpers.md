---
name: libaom/omit-unused-parameters-from-internal-helpers
description: When defining or refactoring internal static helpers, remove unused or obsolete parameters from signatures and call sites, silencing unused arguments only at fixed interface or RTCD wrappers
rubric: design
severity: should_fix
---

**Rule.** When defining or refactoring internal static helper functions, macro
templates, and internal routines unconstrained by external ABIs, eliminate
unused, dead, speculative, or obsolete parameters (such as bit depth bd,
strides, configuration flags, or precalculated values) from signatures,
prototypes, and all call sites rather than suppressing them with (void)param; or
UNUSED(param). For square-only helper operations, accept a single dimension
argument instead of separate width and height, and define invariant constants
locally inside specialized helpers instead of passing dummy constant arguments
from callers. Only retain unused parameters and silence them using (void)param;
at standardized interface boundaries, RTCD function pointers, or macro wrappers
where signatures are strictly constrained by external contracts.

**Why.** Internal helper functions and specialized routines are not constrained
by external fixed ABI signatures or function pointer tables. Retaining dead or
speculative parameters adds visual clutter, increases call-site boilerplate,
misleads maintainers about actual data dependencies, and incurs unnecessary
parameter-passing overhead. Suppressing unused parameters with (void) inside
internal helpers obscures what the function genuinely requires; suppressing them
at interface/RTCD wrappers cleanly demarcates the fixed ABI contract from
internal helper logic.

## Examples

Bad:

```cpp
// Bad: Retaining dead parameters or silencing with (void) inside internal helpers.
static INLINE void flat_mask_internal(const __m128i *th, const __m128i *pq,
                                      int bd, int start, int end,
                                      __m128i *flat) {
  (void)bd;  // 'bd' is obsolete after refactoring precalculated 'th'
  const __m128i f1 = _mm_sub_epi16(_mm_loadu_si128(pq + start), *th);
  // ...
}

// Bad: Propagating unused RTCD wrapper parameter into internal static helper.
static INLINE void highbd_dc_predictor_neon(uint16_t *dst, ptrdiff_t stride,
                                            int bw, int bh,
                                            const uint16_t *above,
                                            const uint16_t *left, int bd) {
  (void)bd;
  (void)bh;  // Redundant dimension in square-only helper
  // ...
}
#define intra_pred_highbd_sized(type, width, height)                        \
  void aom_highbd_##type##_predictor_##width##x##height##_neon(             \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,               \
      const uint16_t *left, int bd) {                                       \
    highbd_##type##_predictor_neon(dst, stride, width, height, above, left, \
                                   bd);                                     \
  }
```

Good:

```cpp
// Good: Omit obsolete parameters from helper signatures and call sites.
static INLINE void flat_mask_internal(const __m128i *th, const __m128i *pq,
                                      int start, int end,
                                      __m128i *flat) {
  const __m128i f1 = _mm_sub_epi16(_mm_loadu_si128(pq + start), *th);
  // ...
}

// Good: Silence fixed wrapper parameter at RTCD level; keep helper minimal.
static INLINE void highbd_dc_predictor_neon(uint16_t *dst, ptrdiff_t stride,
                                            int bw, const uint16_t *above,
                                            const uint16_t *left) {
  // ...
}
#define intra_pred_highbd_sized(type, width)                         \
  void aom_highbd_##type##_predictor_##width##x##width##_neon(       \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,        \
      const uint16_t *left, int bd) {                                \
    (void)bd;                                                        \
    highbd_##type##_predictor_neon(dst, stride, width, above, left); \
  }
```
