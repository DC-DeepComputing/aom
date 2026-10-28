---
name: libaom/aom_dsp/compute-stride-offsets-inline-in-simd
description: When computing multi-row buffer addresses in SIMD intrinsic functions, compute stride offsets inline rather than declaring redundant scalar offset variables like stride3
rubric: readability
severity: nit
---

**Rule.** In C SIMD intrinsic functions, compute multi-row buffer addresses and
stride offsets inline (e.g., `dst + stride * 2`, `dst + stride * 3`) rather than
precalculating redundant scalar offset variables (such as `ptrdiff_t stride3 =
stride * 3`). If auxiliary local variables are genuinely necessary, declare and
initialize them at their point of first use with `const`.

**Why.** In assembly, precomputing `stride * 3` into a dedicated register was
necessary due to x86 addressing mode limitations (scale factors of 1, 2, 4, 8).
In C SIMD intrinsic functions, the compiler optimizes inline address arithmetic
directly; introducing extra scalar offset variables adds unnecessary clutter and
inconsistency when adjacent rows use direct expressions like `stride * 2`.

## Examples

Bad:

```cpp
// Bad: Declaring redundant scalar stride offset variables (assembly artifact)
ptrdiff_t stride3 = stride * 3;

_mm_store_si128((__m128i *)(dst), r0);
_mm_store_si128((__m128i *)(dst + stride), r1);
_mm_store_si128((__m128i *)(dst + stride * 2), r2);
_mm_store_si128((__m128i *)(dst + stride3), r3);
```

Good:

```cpp
// Good: Compute stride offsets inline consistently
_mm_store_si128((__m128i *)(dst), r0);
_mm_store_si128((__m128i *)(dst + stride), r1);
_mm_store_si128((__m128i *)(dst + stride * 2), r2);
_mm_store_si128((__m128i *)(dst + stride * 3), r3);
```
