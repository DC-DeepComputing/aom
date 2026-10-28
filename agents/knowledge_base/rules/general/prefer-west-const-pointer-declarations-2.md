---
name: libaom/prefer-west-const-pointer-declarations-2
description: Place 'const' before the type name ('const Type *', 'const Type &') rather than after ('Type const *') in pointer declarations, parameters, return types, and casts
rubric: readability
severity: should_fix
---

**Rule.** When declaring pointers or references to const data or writing
explicit pointer casts, place the const qualifier before the type name (west
const, e.g., 'const Type *ptr', 'const struct Type *ptr', 'const Type &ref', or
'(const Type *)ptr') rather than after the type name (east const, e.g., 'Type
const *ptr', 'Type const &ref', or '(Type const *)ptr'). Maintain consistent
west const placement across local variable declarations, function parameters,
return types, typedefs, and pointer casts.

**Why.** In libaom and Google C/C++ style, 'west const' ('const Type *') is the
established, overwhelmingly predominant convention (>24,000 occurrences across
the codebase). Using 'east const' ('Type const *') introduces style
inconsistencies across DSP kernels, SIMD intrinsics, and public APIs, deviating
from established project idioms.

## Examples

Bad:

```cpp
// East const in variable declarations, parameters, and SIMD pointer casts
void process_block(int const *input, uint8_t const *ref_buf, const size_t length) {
  const __m128i val = _mm_loadu_si128((__m128i const *)ref_buf);
  ...
}

static INLINE uint16x8x4_t vld1q_u16_x4(uint16_t const *ptr) {
  uint8_t const *ref_buf = src->buffer;
  return ...;
}
```

Good:

```cpp
// West const in variable declarations, parameters, and SIMD pointer casts
void process_block(const int *input, const uint8_t *ref_buf, const size_t length) {
  const __m128i val = _mm_loadu_si128((const __m128i *)ref_buf);
  ...
}

static INLINE uint16x8x4_t vld1q_u16_x4(const uint16_t *ptr) {
  const uint8_t *ref_buf = src->buffer;
  return ...;
}
```
