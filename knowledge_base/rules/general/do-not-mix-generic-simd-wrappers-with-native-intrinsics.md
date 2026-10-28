---
name: libaom/do-not-mix-generic-simd-wrappers-with-native-intrinsics
description: When writing architecture-specific SIMD code, do not mix generic SIMD wrappers (v128, v256) with native intrinsics (__m128i, _mm_*); use native types consistently
rubric: correctness
severity: blocking
---

**Rule.** In architecture-specific SIMD implementations (such as SSE, AVX2, or
NEON source files), do not mix generic SIMD wrapper types and helper functions
(v128, v256, v64, v128_*, v256_*) from aom_dsp/aom_simd.h with native compiler
intrinsics (such as __m128i, __m256i, _mm_*, _mm256_*). Use native vector types
and intrinsic instructions consistently throughout architecture-specific files.

**Why.** On 32-bit Windows builds using MSVC, aom_dsp/aom_simd.h falls back to
struct-based C SIMD types (simd/v256_intrinsics.h) because 32-bit MSVC does not
support vector types in structs as function arguments. Mixing native vector
types (such as __m128i) with v128_* functions causes compiler type-conversion
errors (e.g., MSVC error C2440: cannot convert from '__m128i' to 'v128').

## Examples

Bad:

```cpp
// Mixing native __m128i intrinsics with generic v128_* wrappers
const __m128i first_8x8_output = _mm256_castsi256_si128(max);
const __m128i cmpeg_res_00 =
    v128_cmpeq_32(first_8x8_output, _mm256_castsi256_si128(dir47));
const v128 t_first_8x8 = v128_pack_s32_s16(cmpeg_res_00, cmpeg_res_01);
```

Good:

```cpp
// Consistently using native x86 SIMD types and intrinsics
const __m128i first_8x8_output = _mm256_castsi256_si128(max);
const __m128i cmpeg_res_00 =
    _mm_cmpeq_epi32(first_8x8_output, _mm256_castsi256_si128(dir47));
const __m128i t_first_8x8 = _mm_packs_epi32(cmpeg_res_01, cmpeg_res_00);
```
