---
name: libaom/target-lowest-required-simd-isa-extension
description: When adding or organizing SIMD files, functions, headers, and RTCD definitions, target and label them with the minimum required ISA extension matching the intrinsics actually used
rubric: readability
severity: should_fix
---

**Rule.** Name, organize, and register SIMD source files, function suffixes,
header includes, CMake lists, and RTCD definitions according to the minimum
instruction set architecture (ISA) extension required by the intrinsics actually
used. For example, label and register an implementation as SSE2 rather than
SSE4.1 or AVX2 if it only uses SSE2 intrinsics (such as <emmintrin.h>).

**Why.** Registering SIMD functions under a higher ISA extension than necessary
restricts runtime CPU detection (RTCD), preventing systems that only support the
lower ISA from executing the SIMD fast path. It also misleads maintainers
regarding compiler flags, header dependencies, and minimum CPU requirements.

## Examples

Bad:

```cpp
// In av1/encoder/x86/model_rd_sse4.c:
#include <smmintrin.h>

void av1_interp_cubic_rate_dist_sse4_1(const double *p1, const double *p2,
                                       double x, double *rate_f,
                                       double *distbysse_f) {
  const __m128d half = _mm_set1_pd(0.5);
  // ... only uses SSE2 intrinsics
}

# In av1/common/av1_rtcd_defs.pl:
specialize qw/av1_interp_cubic_rate_dist sse4_1/;
```

Good:

```cpp
// In av1/encoder/x86/model_rd_sse2.c:
#include <emmintrin.h>

void av1_interp_cubic_rate_dist_sse2(const double *p1, const double *p2,
                                     double x, double *rate_f,
                                     double *distbysse_f) {
  const __m128d half = _mm_set1_pd(0.5);
  // ...
}

# In av1/common/av1_rtcd_defs.pl:
specialize qw/av1_interp_cubic_rate_dist sse2/;
```
