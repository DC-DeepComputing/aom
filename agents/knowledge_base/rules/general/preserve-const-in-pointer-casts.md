---
name: libaom/preserve-const-in-pointer-casts
description: When casting pointer types to access or load data from const buffers, preserve the const qualifier in the target pointer type
rubric: readability
severity: nit
---

**Rule.** When casting pointers to load, index, or inspect data from a `const`
buffer or pointer, retain the `const` qualifier in the target pointer type
(e.g., `(const uint32_t *)ptr` or `(const __m128i *)ptr`). Do not cast away
`const` by converting to a non-const pointer type such as `(uint32_t *)ptr` or
`(__m128i *)ptr`.

**Why.** Casting away `const` when reading from immutable buffers weakens
compiler type safety and violates const-correctness conventions. Retaining
`const` across pointer casts and SIMD loads prevents accidental mutation,
clarifies read-only intent, and avoids compiler warnings.

## Examples

Bad:

```cpp
const __m128i weight = _mm_cvtsi32_si128(((uint32_t *)weight_array)[1]);
const __m128i data = _mm_loadu_si128((__m128i *)const_src);
```

Good:

```cpp
const __m128i weight = _mm_cvtsi32_si128(((const uint32_t *)weight_array)[1]);
const __m128i data = _mm_loadu_si128((const __m128i *)const_src);
```
