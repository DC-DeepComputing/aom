---
name: libaom/aom_dsp/place-shared-dsp-headers-in-aom-dsp
description: When defining shared DSP or SIMD helper headers, place them in aom_dsp/ rather than av1/ to prevent reverse dependency includes from aom_dsp into av1
rubric: design
severity: should_fix
---

**Rule.** Place shared DSP and SIMD helper headers in `aom_dsp/` (e.g.,
`aom_dsp/x86/`) rather than in codec-specific directories under `av1/`. Source
files in `aom_dsp/` must never `#include` headers from `av1/`, preserving clean
library layering where `av1` depends on `aom_dsp` and not vice versa.

**Why.** `aom_dsp` is a foundational DSP library that higher-level codec modules
such as `av1` build upon. Placing shared DSP routines or SIMD helper headers in
`av1/` forces `aom_dsp` files to include codec headers, creating an inverted
dependency and breaking modular library layering.

## Examples

Bad:

```cpp
// In aom_dsp/x86/aom_subpixel_8t_intrin_avx2.c
#include "av1/common/x86/convolve_avx2.h"
```

Good:

```cpp
// In aom_dsp/x86/aom_subpixel_8t_intrin_avx2.c
#include "aom_dsp/x86/convolve_avx2.h"
```
