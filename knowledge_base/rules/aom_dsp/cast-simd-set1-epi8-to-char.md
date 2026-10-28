---
name: libaom/aom_dsp/cast-simd-set1-epi8-to-char
description: When passing byte values or mask constants to x86 SIMD intrinsics like _mm_set1_epi8, cast explicitly to char rather than uint8_t
rubric: readability
severity: should_fix
---

**Rule.** When passing byte values, mask expressions, or constants (such as 0x80
or 0xff) to x86 SIMD byte-broadcast or byte-setting intrinsics (such as
_mm_set1_epi8, _mm256_set1_epi8, or _mm_set_epi8), cast them explicitly to
(char) rather than (uint8_t) or (int).

**Why.** x86 byte-setting intrinsics define their parameter types as char.
Passing uint8_t expressions or integer constants exceeding 127 causes
signed/unsigned conversion warnings or integer sanitizer warnings; casting
explicitly to (char) matches the intrinsic's signature.

## Examples

Bad:

```cpp
// Bad: casting to uint8_t or passing uncast values > 127 triggers conversion warnings
__m128i mask = _mm_set1_epi8((uint8_t)(0xff >> c));
__m128i t80 = _mm_set1_epi8((uint8_t)0x80);
```

Good:

```cpp
// Good: explicitly cast to (char) to match intrinsic parameter signature
__m128i mask = _mm_set1_epi8((char)(0xff >> c));
__m128i t80 = _mm_set1_epi8((char)0x80);
```
