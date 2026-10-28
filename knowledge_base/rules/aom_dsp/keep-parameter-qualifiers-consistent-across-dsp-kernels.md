---
name: libaom/aom_dsp/keep-parameter-qualifiers-consistent-across-dsp-kernels
description: When modifying function signatures across sibling DSP kernel variants, apply parameter qualifiers like LIBAOM_RESTRICT and const consistently across all block sizes
rubric: correctness
severity: should_fix
---

**Rule.** When adding or modifying function signatures across a family of
sibling DSP kernels (such as block-size variants like 16x4, 16x8, 16x16, or
predictor routines), ensure parameter qualifiers (such as LIBAOM_RESTRICT,
restrict, or const) are applied consistently across every variant. Do not omit
aliasing qualifiers or pointer constness from individual functions within the
family.

**Why.** Sibling DSP kernel families share identical parameter lists, memory
layouts, and pointer aliasing contracts across block sizes. Inadvertently
omitting aliasing qualifiers like LIBAOM_RESTRICT or restrict on specific
variants prevents compiler optimizations and introduces interface
inconsistencies across related kernels.

## Examples

Bad:

```cpp
void aom_smooth_h_predictor_16x4_ssse3(
    uint8_t *dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column);
void aom_smooth_h_predictor_16x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column);
```

Good:

```cpp
void aom_smooth_h_predictor_16x4_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column);
void aom_smooth_h_predictor_16x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column);
```
