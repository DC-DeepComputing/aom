---
name: libaom/use-extern-for-cross-unit-forward-declarations
description: When sharing functions, constant tables, or variables across translation units, declare them in common headers rather than placing ad-hoc extern declarations in source files
rubric: design
severity: should_fix
---

**Rule.** When sharing function prototypes, constant tables (e.g.
`DECLARE_ALIGNED`), or global variables across translation units or SIMD
backends, declare them in a dedicated subsystem `.h` header file (qualifying
variables/tables with `extern`) and `#include` that header in source files. Do
not place ad-hoc `extern` variable declarations or duplicate function forward
declarations directly inside individual `.c` source files. When
forward-declaring cross-unit functions in C source files where no shared header
exists, explicitly qualify the prototype with `extern`.

**Why.** Declaring shared functions, constants, and variables in a common header
provides a single source of truth across all translation units. This allows the
compiler to enforce type signature, alignment, and parameter consistency between
the definition and all call sites, avoiding duplicate forward declarations, type
mismatches, and undefined symbol bugs across architecture-specific source files.

## Examples

Bad:

```cpp
// Variant 1: Ad-hoc extern constant tables in .c files
// In av1/common/arm/convolve_neon_i8mm.c:
DECLARE_ALIGNED(16, extern const uint8_t, kDotProdPermuteTbl[48]);
DECLARE_ALIGNED(16, extern const uint8_t, kDotProdMergeBlockTbl[48]);

// Variant 2: Duplicate function forward declarations in .c files
// In aom_dsp/x86/fft_avx2.c:
void aom_fft1d_8_float(const float *input, float *output, int stride);
void aom_transpose_float_sse2(const float *A, float *B, int n);
```

Good:

```cpp
// Variant 1: Declare extern constant tables in dedicated header
// In av1/common/arm/convolve_neon_dotprod.h:
DECLARE_ALIGNED(16, extern const uint8_t, kDotProdPermuteTbl[48]);
DECLARE_ALIGNED(16, extern const uint8_t, kDotProdMergeBlockTbl[48]);
// In av1/common/arm/convolve_neon_i8mm.c:
#include "av1/common/arm/convolve_neon_dotprod.h"

// Variant 2: Declare shared functions in subsystem header
// In aom_dsp/fft_common.h:
void aom_fft1d_8_float(const float *input, float *output, int stride);
extern void aom_transpose_float_sse2(const float *A, float *B, int n);
// In aom_dsp/x86/fft_avx2.c:
#include "aom_dsp/fft_common.h"
```
