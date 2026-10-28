---
name: libaom/aom_dsp/migrate-neutral-performance-kernels-to-highway
description: When porting DSP or codec kernels to Highway SIMD, migrate operations with neutral or equivalent performance alongside speedups to simplify arch-specific paths
rubric: design
severity: should_fix
---

**Rule.** When porting DSP routines and coding kernels to portable Highway SIMD,
do not omit or exclude operations that exhibit neutral, similar, or equivalent
performance compared to architecture-specific intrinsics. Include
neutral-performing operations alongside clear speedups to retire target-specific
implementations, unify code across SIMD backends, and maximize automated test
coverage across targets.

**Why.** Target-specific SIMD routines (e.g., AVX2 or NEON intrinsics) require
ongoing maintenance and separate test coverage across different hardware
configurations. Migrating neutral-or-better performing kernels to portable
Highway SIMD enables deprecating target-specific handwritten paths, consolidates
test validation, and ensures uniform behavior across all supported architectures
without regressing performance.

## Examples

Bad:

```cpp
// Omitting 3D/4D variants from Highway migration due to lack of speedup:
// FSAD_4D, FSAD_3D and FSAD_4D_SKIP have no obvious improvement
FOR_EACH_SAD_BLOCK_SIZE(FSAD, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_SKIP, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_AVG, avx2)
```

Good:

```cpp
// Migrating neutral-performance variants to unify code paths and test coverage:
// FSAD_4D, FSAD_3D and FSAD_4D_SKIP have similar performance, but migrating
// to highway adds test coverage and simplifies avx2 code paths.
FOR_EACH_SAD_BLOCK_SIZE(FSAD, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_SKIP, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_AVG, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_4D, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_4D_SKIP, avx2)
FOR_EACH_SAD_BLOCK_SIZE(FSAD_3D, avx2)
```
