---
name: libaom/narrow-ifdef-scope-to-diverging-logic
description: 'When writing platform- or compiler-specific logic, narrow #if/#else guards to inline helper functions or inner blocks instead of duplicating scaffolding or embedding conditionals in algorithms'
rubric: design
severity: should_fix
---

**Rule.** Do not duplicate entire function definitions, caller logic, or outer
loop scaffolding across architecture-, platform-, or feature-specific #if/#else
preprocessor branches (such as #if AOM_ARCH_AARCH64 or #if
defined(__ARM_FEATURE_DOTPROD)), and do not embed compiler-specific intrinsic
conditionals (__builtin_*, MSVC _byteswap_* / BitScan*, #pragma intrinsic)
directly inside core algorithm functions. Instead, consolidate shared parameter
setup, pointer offsets, and iteration scaffolding into unconditionally called
code, and isolate platform or compiler divergence inside dedicated static
AOM_INLINE helper functions or narrowed inner #if/#else blocks.

**Why.** Inlining preprocessor conditionals inside algorithm flow obscures code
readability and creates maintenance hazards. Similarly, duplicating outer
functions or loop scaffolding across preprocessor branches inflates binary
maintenance overhead and leads to subtle divergence when bug fixes, offsets, or
setup logic are updated in one branch but omitted in another. Encapsulating
compiler intrinsics and platform variants into dedicated inline helper functions
localizes target differences cleanly and preserves shared algorithmic
scaffolding without runtime overhead.

## Examples

Bad:

```cpp
// Variant 1: Inlining compiler intrinsic conditionals and fallbacks directly in algorithm logic
void encode_data(uint8_t *out, uint32_t offs, uint64_t output, uint8_t nbytes) {
#if defined(__GNUC__)
  const uint64_t reg = __builtin_bswap64(output) >> ((8 - nbytes) << 3);
  memcpy(&out[offs], &reg, 8);
#elif defined(USE_MSC_INTRINSICS)
  const uint64_t reg = _byteswap_uint64(output) >> ((8 - nbytes) << 3);
  memcpy(&out[offs], &reg, 8);
#else
  for (int i = 0; i < nbytes; ++i) out[offs + i] = (output >> (56 - (i << 3))) & 0xff;
#endif
}

// Variant 2: Duplicating outer loop scaffolding across feature branches
#if defined(__ARM_FEATURE_DOTPROD)
static INLINE uint32_t sse_128xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride, int h) {
  uint32x4_t sse = vdupq_n_u32(0);
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < 128; j += 16) sse_16x1_neon(src + j, ref + j, &sse);
    src += src_stride; ref += ref_stride;
  }
  return horizontal_add_u32x4(sse);
}
#else
static INLINE uint32_t sse_128xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride, int h) {
  // Duplicated outer loop scaffolding identical to the dotprod branch
  uint32x4_t sse = vdupq_n_u32(0);
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < 128; j += 16) sse_16x1_neon(src + j, ref + j, &sse);
    src += src_stride; ref += ref_stride;
  }
  return horizontal_add_u32x4(sse);
}
#endif
```

Good:

```cpp
// Variant 1: Encapsulate compiler-specific intrinsics and fallbacks in a static inline helper
static AOM_INLINE void write_enc_data_to_out_buf(uint8_t *out, uint32_t offs,
                                                 uint64_t output, uint8_t nbytes) {
#if defined(__GNUC__)
  const uint64_t reg = __builtin_bswap64(output) >> ((8 - nbytes) << 3);
  memcpy(&out[offs], &reg, 8);
#elif defined(USE_MSC_INTRINSICS)
  const uint64_t reg = _byteswap_uint64(output) >> ((8 - nbytes) << 3);
  memcpy(&out[offs], &reg, 8);
#else
  for (int i = 0; i < nbytes; ++i) out[offs + i] = (output >> (56 - (i << 3))) & 0xff;
#endif
}

// Variant 2: Guard only low-level helpers; keep caller loops and scaffolding unconditional
#if defined(__ARM_FEATURE_DOTPROD)
static INLINE void sse_16x1_neon(const uint8_t *src, const uint8_t *ref, uint32x4_t *sse) { ... }
#else
static INLINE void sse_16x1_neon(const uint8_t *src, const uint8_t *ref, uint32x4_t *sse) { ... }
#endif

static INLINE uint32_t sse_128xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride, int h) {
  uint32x4_t sse = vdupq_n_u32(0);
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < 128; j += 16) sse_16x1_neon(src + j, ref + j, &sse);
    src += src_stride; ref += ref_stride;
  }
  return horizontal_add_u32x4(sse);
}
```
