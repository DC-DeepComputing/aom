---
name: libaom/aom_dsp/consolidate-block-dimension-functions-using-macros
description: When implementing families of DSP or SIMD functions across multiple block dimensions with identical logic, consolidate repetitive definitions into parameterized macros
rubric: design
severity: nit
---

**Rule.** When defining families of DSP or SIMD functions across multiple 2D
block dimensions (such as predictors, filters, SAD, or variance routines) that
share identical function bodies, consolidate repetitive function definitions
into parameterized macros rather than handwriting each width-by-height variant.
Parameterize the macro by dimension parameters, shift values, or vector register
qualifiers, instantiate all required block sizes, and #undef the macro
immediately after instantiation.

**Why.** Handwriting dozens of identical width-by-height function variants
causes severe code duplication, bloats source files, and increases maintenance
risk when algorithms or helper signatures change. Parameterized generator macros
keep kernel files concise, ensure uniform implementation across all block sizes,
and avoid copy-paste errors.

## Examples

Bad:

```cpp
void aom_dc_128_predictor_4x8_neon(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  dc_store_4xh(dst, stride, 8, vdup_n_u8(128));
}

void aom_dc_128_predictor_8x4_neon(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  (void)above;
  (void)left;
  dc_store_8xh(dst, stride, 4, vdup_n_u8(128));
}
```

Good:

```cpp
#define DC_PREDICTOR_128(w, h, q)                                             \
  void aom_dc_128_predictor_##w##x##h##_neon(uint8_t *dst, ptrdiff_t stride, \
                                             const uint8_t *above,            \
                                             const uint8_t *left) {           \
    (void)above;                                                              \
    (void)left;                                                               \
    dc_store_##w##xh(dst, stride, (h), vdup##q##_n_u8(0x80));                 \
  }

DC_PREDICTOR_128(4, 8, )
DC_PREDICTOR_128(8, 4, )
#undef DC_PREDICTOR_128
```
