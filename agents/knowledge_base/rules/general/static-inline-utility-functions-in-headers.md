---
name: libaom/static-inline-utility-functions-in-headers
description: Define trivial, hot helpers as static inline in header files; declare non-trivial, lengthy, or looping functions in headers and implement them in .c files
rubric: design
severity: should_fix
---

**Rule.** Define small, trivial, or performance-critical helper functions (such
as pointer casts, type conversions, simple arithmetic clamps, accessors, or
context caching) as static inline (or static INLINE, static AOM_INLINE, static
AOM_FORCE_INLINE, SIMD_INLINE) in header files to enable cross-translation-unit
compiler inlining. Conversely, do not define lengthy, complex, looping,
lifecycle, or non-critical functions—nor plain non-inline static
functions—inside header files; declare their prototypes in header files and
place their definitions in corresponding .c source files.

**Why.** Inlining small, trivial helpers (like pointer casts or field accessors)
in header files eliminates function call overhead in performance-critical inner
loops. Conversely, placing lengthy, complex, or non-critical function
definitions in header files clutters header interfaces, increases compilation
times across translation units, and risks binary bloat or code duplication when
inlining thresholds are exceeded. Furthermore, placing plain static (non-inline)
function definitions in headers causes unused-function compiler warnings and
redundant per-translation-unit object code duplication.

## Examples

Bad:

```cpp
// Variant 1: Small trivial casting/accessor helper defined as non-inline in .c
// In av1/common/idct.c
const int32_t *av1_cast_to_int32(const tran_low_t *input) {
  assert(sizeof(int32_t) == sizeof(tran_low_t));
  return (const int32_t *)input;
}

// Variant 2: Lengthy/loop/SIMD routine defined as static inline in header
// In av1/common/convolve.h
static INLINE void av1_convolve_x_sr_12tap(const uint8_t *src, int src_stride,
                                           uint8_t *dst, int dst_stride, int w,
                                           int h, const InterpFilterParams *p) {
  // ~50 lines of complex loop / multi-tap filtering logic
}

// Variant 3: Plain static (non-inline) function defined in header
// In av1/encoder/encodeframe_utils.h
static void av1_dealloc_src_diff_buf(struct macroblock *mb, int num_planes) {
  for (int plane = 0; plane < num_planes; ++plane) {
    aom_free(mb->plane[plane].src_diff);
    mb->plane[plane].src_diff = NULL;
  }
}
```

Good:

```cpp
// Variant 1: Small trivial helper defined as static inline in header
// In av1/common/idct.h
static inline const int32_t *cast_to_int32(const tran_low_t *input) {
  assert(sizeof(int32_t) == sizeof(tran_low_t));
  return (const int32_t *)input;
}

// Variant 2: Lengthy/complex logic declared in header, defined in source (.c)
// In av1/common/convolve.h
void av1_convolve_x_sr_12tap(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *p);
// In av1/common/convolve.c
void av1_convolve_x_sr_12tap(...) {
  // Complex implementation defined out-of-line
}

// Variant 3: Prototype declared in header, defined in .c
// In av1/encoder/encodeframe_utils.h
void av1_dealloc_src_diff_buf(struct macroblock *mb, int num_planes);
```
