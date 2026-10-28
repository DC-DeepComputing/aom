---
name: libaom/use-mm-loadl-epi64-not-loadu-si64-in-sse2
description: When loading 64-bit integers into 128-bit registers in SSE2 code using <emmintrin.h>, use _mm_loadl_epi64 instead of _mm_loadu_si64 to avoid MSVC build failures
rubric: correctness
severity: blocking
---

**Rule.** In SSE2 x86 SIMD routines and headers that include <emmintrin.h>, use
standard SSE2 intrinsics such as _mm_loadl_epi64 (or helper functions like
xx_loadl_64) rather than _mm_loadu_si64. Do not use _mm_loadu_si64 in SSE2-level
translation units.

**Why.** <emmintrin.h> defines baseline SSE2 intrinsics like _mm_loadl_epi64,
whereas _mm_loadu_si64 is declared in <immintrin.h>. Compilers with strict
intrinsic header partitioning such as MSVC fail compilation with undeclared
identifier errors when _mm_loadu_si64 is used without including <immintrin.h>.

## Examples

Bad:

```cpp
#include <emmintrin.h>

static INLINE __m128i xx_loadu_2x64(const void *hi, const void *lo) {
  return _mm_unpacklo_epi64(_mm_loadu_si64(lo), _mm_loadu_si64(hi));
}
```

Good:

```cpp
#include <emmintrin.h>

static INLINE __m128i xx_loadu_2x64(const void *hi, const void *lo) {
  return _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)lo),
                            _mm_loadl_epi64((const __m128i *)hi));
}
```
