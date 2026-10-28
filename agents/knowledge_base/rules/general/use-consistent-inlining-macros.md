---
name: libaom/use-consistent-inlining-macros
description: When declaring helper functions, match surrounding inlining style (INLINE, AOM_INLINE, or static inline), avoid unjustified AOM_FORCE_INLINE, and omit INLINE on non-trivial static helpers
rubric: readability
severity: should_fix
---

**Rule.** When declaring inline functions or internal helpers, use inlining
macros (`INLINE`, `AOM_INLINE`, or `static inline`) consistent with the
surrounding file. Avoid `AOM_FORCE_INLINE` (`__attribute__((always_inline))` /
`__forceinline`) unless aggressive forced inlining is proven strictly necessary
for performance-critical inner loops or hotspots. Additionally, in `.c` source
files, omit `INLINE` on non-trivial `static` helper functions and declare them
as plain `static`, allowing compiler optimization heuristics to manage inlining.

**Why.** Standardizing on file-level inlining conventions maintains codebase
consistency. Standard `static inline` or plain `static` allows the compiler to
make optimal inlining decisions based on optimization level and code size.
Forcing inlining with `AOM_FORCE_INLINE` or marking non-trivial `.c` helpers
with `INLINE` causes unnecessary binary bloat and instruction cache pressure.

## Examples

Bad:

```cpp
// Bad: unjustified AOM_FORCE_INLINE on routine internal helper
static AOM_FORCE_INLINE unsigned int obmc_sad_w4_avx2(const uint8_t *pre,
                                                      const int pre_stride,
                                                      const int32_t *wsrc,
                                                      const int32_t *mask,
                                                      const int height) {
  // Routine helper logic...
}

// Bad: marking non-trivial static C helper with INLINE
static INLINE void build_non_directional_intra_predictors(
    const uint8_t *ref, int ref_stride, uint8_t *dst, int dst_stride,
    PREDICTION_MODE mode, TX_SIZE tx_size, int n_top_px, int n_left_px) {
  // Non-trivial intra prediction logic...
}
```

Good:

```cpp
// Good: standard static inline (or matching INLINE macro) without forced inlining
static inline unsigned int obmc_sad_w4_avx2(const uint8_t *pre,
                                            const int pre_stride,
                                            const int32_t *wsrc,
                                            const int32_t *mask,
                                            const int height) {
  // Routine helper logic...
}

// Good: plain static declaration for non-trivial helper in .c file
static void build_non_directional_intra_predictors(
    const uint8_t *ref, int ref_stride, uint8_t *dst, int dst_stride,
    PREDICTION_MODE mode, TX_SIZE tx_size, int n_top_px, int n_left_px) {
  // Non-trivial intra prediction logic...
}
```
