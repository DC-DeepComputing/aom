---
name: libaom/return-single-values-and-remove-unused-out-params
description: Return single computed scalar values and SIMD vectors directly by value instead of out-pointers, and eliminate unused out-parameters or dead calculations.
rubric: design
severity: should_fix
---

**Rule.** When defining or refactoring functions (including SIMD helper, load,
and transformation functions), return single computed scalar or vector results
(such as int, uint64_t, uint8x8_t, uint16x8_t, int16x8_t, __m128i, or v128_t)
directly by value rather than through output pointer parameters. Reserve pointer
output parameters for functions producing multiple outputs or aggregate
multi-vector block loads (e.g., 4x4 or 4x8 loads). Additionally, if callers do
not read an out-parameter, remove the parameter from signatures and call sites,
declaring any required intermediate state locally within the callee and
eliminating dead computations.

**Why.** Returning single scalar values and hardware-register-sized SIMD vectors
directly by value clarifies data flow, eliminates pointer indirection,
simplifies function signatures, makes helpers composable in expressions, and
clearly distinguishes single-result functions from multi-output or multi-vector
aggregate loaders. Removing unused out-parameters avoids unnecessary address-of
operations, simplifies call sites, and eliminates CPU cycles wasted on dead
calculations.

## Examples

Bad:

```cpp
// Bad: Single vector result written via output pointer parameter.
static INLINE void load_unaligned_u8_2x2(const uint8_t *buf, ptrdiff_t stride,
                                         uint8x8_t *tu0) {
  const uint16_t a_u16 = *(const uint16_t *)buf;
  *tu0 = vreinterpret_u8_u16(vdup_n_u16(a_u16));
}

// Bad: Single computed scalar passed via out-pointer; unused out-param passed.
static void compute_sse(const uint8_t *a, int a_stride, const uint8_t *b,
                        int b_stride, int w, int h, uint64_t *sse) {
  *sse = 0;
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      const int diff = a[j] - b[j];
      *sse += diff * diff;
    }
  }
}
int unused_pred_flag = 0;
set_block_search_params(cpi, x, &search_state, cost, &unused_pred_flag);
```

Good:

```cpp
// Good: Return the single vector result directly by value.
static INLINE uint8x8_t load_unaligned_u8_2x2(const uint8_t *buf,
                                             ptrdiff_t stride) {
  const uint16_t a_u16 = *(const uint16_t *)buf;
  return vreinterpret_u8_u16(vdup_n_u16(a_u16));
}

// Good: Return scalar directly; manage intermediate state locally.
static uint64_t compute_sse(const uint8_t *a, int a_stride, const uint8_t *b,
                            int b_stride, int w, int h) {
  uint64_t sse = 0;
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      const int diff = a[j] - b[j];
      sse += diff * diff;
    }
  }
  return sse;
}
set_block_search_params(cpi, x, &search_state, cost);
```
