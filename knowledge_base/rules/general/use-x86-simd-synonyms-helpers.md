---
name: libaom/use-x86-simd-synonyms-helpers
description: When implementing x86 SIMD routines, use standardized load/store helpers from synonyms.h, synonyms_avx2.h, or mem_sse2.h instead of custom wrappers or ad-hoc memcpy
rubric: readability
severity: should_fix
---

**Rule.** In x86 SIMD routines (SSE/AVX), use standardized intrinsic shorthand
helpers from `aom_dsp/x86/synonyms.h` (e.g., `xx_loadl_32`, `xx_loadl_64`,
`xx_loadu_128`, `xx_storel_16`, `xx_storel_32`, `xx_storel_64`) and
`aom_dsp/x86/synonyms_avx2.h` (e.g., `yy_loadu_256`, `yy_storeu_256`) for vector
memory loads/stores, and use unaligned scalar load helpers from
`aom_dsp/x86/mem_sse2.h` (e.g., `loadu_int16`, `loadu_int32`, `loadu_int64`) for
scalar integers. Do not hand-roll unaligned scalar loads with temporary
variables and manual `memcpy`, define custom file-local load/store wrapper
functions, or open-code ad-hoc inline register extraction; centralize missing
wrappers in `synonyms.h`.

**Why.** libaom centralizes safe, unaligned, and partial-vector memory access
helpers in `synonyms.h` (`xx_*`), `synonyms_avx2.h` (`yy_*`), and `mem_sse2.h`
(`loadu_int*`). Reusing these standardized helpers eliminates boilerplate
temporary variables, avoids strict-aliasing and alignment pitfalls from ad-hoc
`memcpy` or manual pointer casting, and ensures uniform SIMD coding style across
optimization kernels.

## Examples

Bad:

```cpp
// Anti-pattern 1: Defining local helper functions with memcpy
static inline __m128i Load4(const void *src) {
  int val;
  memcpy(&val, src, sizeof(val));
  return _mm_cvtsi32_si128(val);
}
const __m128i top = Load4(top_ptr - 1);

// Anti-pattern 2: Ad-hoc inline sub-register extraction and memcpy
int16_t val = (int16_t)_mm_cvtsi128_si32(res);
memcpy(dst_ptr, &val, sizeof(val));

// Anti-pattern 3: Manual memcpy into temporary variables for vector set
int32_t src0, src1;
memcpy(&src0, src + 0 * stride, sizeof(src0));
memcpy(&src1, src + 1 * stride, sizeof(src1));
__m256i s = _mm256_setr_epi32(src0, src1, 0, 0, 0, 0, 0, 0);
```

Good:

```cpp
#include "aom_dsp/x86/mem_sse2.h"
#include "aom_dsp/x86/synonyms.h"

// Variant 1: Standard load helper
const __m128i top = xx_loadl_32(top_ptr - 1);

// Variant 2: Standard sub-register store helper
xx_storel_16(dst_ptr, res);

// Variant 3: Standard unaligned scalar load helper
__m256i s = _mm256_setr_epi32(loadu_int32(src + 0 * stride),
                             loadu_int32(src + 1 * stride),
                             0, 0, 0, 0, 0, 0);
```
