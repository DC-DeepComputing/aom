---
name: libaom/c-function-names-lowercase-snake-case
description: When declaring or defining C functions in libaom source files, name them using lowercase snake_case rather than PascalCase or camelCase
rubric: readability
severity: should_fix
---

**Rule.** When declaring or defining functions in C source or header files (.c,
.h), name them in all-lowercase snake_case (e.g., filter_4x2_sse4_1,
aom_convolve8_horiz_c), not PascalCase (Filter4x2_SSE4_1) or camelCase. When
porting code from C++ codebases (such as libgav1 or Chromium), adapt all
function identifiers to libaom's lowercase snake_case style.

**Why.** Consistent snake_case naming adheres to libaom and Google C coding
conventions across C subsystems, distinguishes functions from macros and enum
constants, and prevents stylistic inconsistencies when porting SIMD or helper
functions from C++ codebases.

## Examples

Bad:

```cpp
// Non-idiomatic PascalCase / CamelCase function names
static inline void Filter4x2_SSE4_1(uint8_t *dst, const ptrdiff_t stride,
                                    const __m128i *pixels, const int128 *weights);

void ProcessBlock_AVX2(uint8_t *dst, int stride, int w, int h);
```

Good:

```cpp
// Idiomatic lowercase snake_case matching libaom C conventions
static inline void filter_4x2_sse4_1(uint8_t *dst, const ptrdiff_t stride,
                                     const __m128i *pixels, const int128 *weights);

void process_block_avx2(uint8_t *dst, int stride, int w, int h);
```
