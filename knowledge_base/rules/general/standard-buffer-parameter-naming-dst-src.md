---
name: libaom/standard-buffer-parameter-naming-dst-src
description: When declaring buffer pointer and stride parameters in functions or RTCD prototypes, use standard abbreviations dst and src rather than dest or source
rubric: readability
severity: nit
---

**Rule.** When declaring buffer pointer and stride parameters in functions,
helper routines, or RTCD prototypes, use the established abbreviations `dst` and
`src` (and `dst_stride` / `src_stride`) rather than full or non-standard
variations like `dest`, `source`, `destination`, or `dest_stride`. Maintain this
naming consistently across C definitions, SIMD implementations, and header
declarations.

**Why.** Standardizing on `dst` and `src` buffer naming aligns with libaom
conventions, RTCD function prototype definitions, and neighboring DSP helpers,
making buffer roles immediately clear and searches across implementations
uniform.

## Examples

Bad:

```cpp
static AOM_FORCE_INLINE void write_smooth_directional_sum16(
    uint8_t *LIBAOM_RESTRICT dest, const uint8_t *source, int dest_stride,
    const __m128i pixels1, const __m128i pixels2, const __m128i weights1,
    const __m128i round) {
  // ...
}
```

Good:

```cpp
static AOM_FORCE_INLINE void write_smooth_directional_sum16(
    uint8_t *LIBAOM_RESTRICT dst, const uint8_t *src, int dst_stride,
    const __m128i pixels1, const __m128i pixels2, const __m128i weights1,
    const __m128i round) {
  // ...
}
```
