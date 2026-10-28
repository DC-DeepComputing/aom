---
name: libaom/aom_dsp/factor-simd-height-loops-into-inline-helpers
description: When implementing SIMD or DSP kernels across block sizes that share identical width logic, factor repetitive loops into inline helpers parameterized by height
rubric: design
severity: should_fix
---

**Rule.** When implementing SIMD or DSP kernels across multiple block sizes that
share identical width and vector operations (such as intra predictors or filters
differing only by height), factor the shared loop body into a single static
inline helper parameterized by height (e.g., `<name>_<width>xh`). Have each
size-specific public kernel invoke the helper with a constant height argument to
avoid duplicating vector load/store logic while enabling compiler inlining and
loop unrolling.

**Why.** Duplicating identical SIMD load, compute, and store loops across
functions of different block heights introduces boilerplate and increases
maintenance burden. Factoring into a static inline helper eliminates duplication
while allowing compilers to inline and optimize or unroll the constant-height
loop in each specialized variant.

## Examples

Bad:

```cpp
void aom_v_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i row0 = _mm_load_si128((__m128i const *)above);
  const __m128i row1 = _mm_load_si128((__m128i const *)(above + 16));
  const __m128i row2 = _mm_load_si128((__m128i const *)(above + 32));
  const __m128i row3 = _mm_load_si128((__m128i const *)(above + 48));
  (void)left;
  for (int i = 0; i < 64; ++i) {
    _mm_store_si128((__m128i *)dst, row0);
    _mm_store_si128((__m128i *)(dst + 16), row1);
    _mm_store_si128((__m128i *)(dst + 32), row2);
    _mm_store_si128((__m128i *)(dst + 48), row3);
    dst += stride;
  }
}

void aom_v_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  const __m128i row0 = _mm_load_si128((__m128i const *)above);
  const __m128i row1 = _mm_load_si128((__m128i const *)(above + 16));
  const __m128i row2 = _mm_load_si128((__m128i const *)(above + 32));
  const __m128i row3 = _mm_load_si128((__m128i const *)(above + 48));
  (void)left;
  for (int i = 0; i < 32; ++i) {
    _mm_store_si128((__m128i *)dst, row0);
    _mm_store_si128((__m128i *)(dst + 16), row1);
    _mm_store_si128((__m128i *)(dst + 32), row2);
    _mm_store_si128((__m128i *)(dst + 48), row3);
    dst += stride;
  }
}
```

Good:

```cpp
static inline void v_predictor_64xh(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, int height) {
  const __m128i row0 = _mm_load_si128((__m128i const *)above);
  const __m128i row1 = _mm_load_si128((__m128i const *)(above + 16));
  const __m128i row2 = _mm_load_si128((__m128i const *)(above + 32));
  const __m128i row3 = _mm_load_si128((__m128i const *)(above + 48));
  for (int i = 0; i < height; ++i) {
    _mm_store_si128((__m128i *)dst, row0);
    _mm_store_si128((__m128i *)(dst + 16), row1);
    _mm_store_si128((__m128i *)(dst + 32), row2);
    _mm_store_si128((__m128i *)(dst + 48), row3);
    dst += stride;
  }
}

void aom_v_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_predictor_64xh(dst, stride, above, 64);
}

void aom_v_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left) {
  (void)left;
  v_predictor_64xh(dst, stride, above, 32);
}
```
