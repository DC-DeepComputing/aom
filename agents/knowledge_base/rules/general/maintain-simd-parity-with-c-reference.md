---
name: libaom/maintain-simd-parity-with-c-reference
description: When modifying architecture-specific SIMD implementations, maintain structural and logical parity with reference C functions rather than introducing isolated drive-by simplifications
rubric: change_scope
severity: should_fix
---

**Rule.** When implementing or updating architecture-specific SIMD functions,
maintain structural and logical parity with the corresponding reference C
implementation (*_c). Do not introduce localized drive-by cleanups or
simplifications in a single SIMD file that cause it to diverge structurally from
the reference C logic; apply shared simplifications and refactorings
consistently across the reference C code and all architecture variants in a
dedicated change.

**Why.** Preserving structural parity between reference C functions and
architecture-optimized SIMD implementations allows reviewers and maintainers to
easily audit, compare, and debug logic across platforms. Localized
simplifications create divergence between architectures, obscuring algorithmic
differences and complicating maintenance.

## Examples

Bad:

```cpp
// In av1/encoder/arm/neon/highbd_temporal_filter_neon.c:
// Localized simplification diverging from reference C (av1_apply_temporal_filter_c):
const int ss_x_shift = mbd->plane[plane].subsampling_x;
const int ss_y_shift = mbd->plane[plane].subsampling_y;
```

Good:

```cpp
// In av1/encoder/arm/neon/highbd_temporal_filter_neon.c:
// Preserving structural and calculation parity with av1_apply_temporal_filter_c:
const int ss_x_shift =
    mbd->plane[plane].subsampling_x - mbd->plane[AOM_PLANE_Y].subsampling_x;
const int ss_y_shift =
    mbd->plane[plane].subsampling_y - mbd->plane[AOM_PLANE_Y].subsampling_y;
```
