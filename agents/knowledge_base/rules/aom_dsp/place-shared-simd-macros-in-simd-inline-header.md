---
name: libaom/aom_dsp/place-shared-simd-macros-in-simd-inline-header
description: When defining shared SIMD helper macros or internal intrinsics, place macros in aom_dsp/aom_simd_inline.h and avoid including top-level aom_simd.h from internal simd headers
rubric: design
severity: should_fix
---

**Rule.** In the SIMD intrinsics hierarchy, define shared helper macros (such as
SIMD_CLAMP or SIMD_INLINE) in aom_dsp/aom_simd_inline.h rather than
aom_dsp/aom_simd.h. Internal intrinsic headers (aom_dsp/simd/*) must never
include top-level aom_dsp/aom_simd.h.

**Why.** aom_dsp/aom_simd.h is the top-level dispatcher header that includes
aom_simd_inline.h followed by backend-specific intrinsic headers. Including
aom_simd.h from internal intrinsic headers creates cyclic dependencies and
breaks standalone test harnesses like test/simd_impl.h that include individual
intrinsic headers directly.

## Examples

Bad:

```cpp
// In aom_dsp/simd/v64_intrinsics_c.h
#include "aom_dsp/aom_simd.h"
```

Good:

```cpp
// In aom_dsp/aom_simd_inline.h
#define SIMD_CLAMP(value, min, max) \
  ((value) > (max) ? (max) : (value) < (min) ? (min) : (value))
```
