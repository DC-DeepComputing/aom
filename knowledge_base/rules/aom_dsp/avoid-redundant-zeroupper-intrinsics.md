---
name: libaom/aom_dsp/avoid-redundant-zeroupper-intrinsics
description: When writing AVX2 or AVX-512 SIMD intrinsic functions in x86 optimization code, avoid calling _mm256_zeroupper() manually because compilers automatically emit vzeroupper.
rubric: readability
severity: should_fix
---

**Rule.** Do not manually call `_mm256_zeroupper()` (or `_mm512_zeroupper()`) in
C AVX2/AVX-512 SIMD intrinsic functions unless strictly necessary. Compilers
automatically generate `vzeroupper` instructions when compiling AVX intrinsic
functions or transitioning between AVX and SSE code.

**Why.** Modern compilers automatically handle `vzeroupper` generation around
AVX intrinsic code and function epilogues. Explicit manual calls to
`_mm256_zeroupper()` add code clutter and redundant instructions.

## Examples

Bad:

```cpp
_mm256_zeroupper();
return *sse - (uint32_t)(((int64_t)se * se) >> 13);
```

Good:

```cpp
return *sse - (uint32_t)(((int64_t)se * se) >> 13);
```
