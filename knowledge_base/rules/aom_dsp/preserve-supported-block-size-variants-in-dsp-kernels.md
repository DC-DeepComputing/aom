---
name: libaom/aom_dsp/preserve-supported-block-size-variants-in-dsp-kernels
description: When modifying or refactoring DSP SIMD kernels or macro instantiations, preserve all supported block-size variants unless deliberately deprecating them across RTCD and tests
rubric: correctness
severity: should_fix
---

**Rule.** When refactoring, optimizing, or adding DSP SIMD kernels or
code-generation macros, do not accidentally omit or remove existing supported
block-size variants (such as 8x8, 8x16, 16x8, or other rectangular and square
sizes). Retain all required macro instantiations and function specializations
unless the block size is intentionally deprecated across RTCD tables, headers,
and test suites.

**Why.** Accidentally deleting or omitting block-size macro instantiations
removes optimized SIMD paths for valid block sizes, causing silent performance
regressions when callers fall back to generic C implementations or triggering
unresolved symbol errors during RTCD dispatch linking.

## Examples

Bad:

```cpp
// Inadvertently omitting 8x8 or 8x16 when refactoring variance macro instantiations:
VAR_FN(16, 16, 16, 8);
VAR_FN(16, 8, 8, 7);
VAR_FN(16, 4, 16, 6);
```

Good:

```cpp
// Retaining all supported square and rectangular block-size variants:
VAR_FN(16, 16, 16, 8);
VAR_FN(16, 8, 8, 7);
VAR_FN(8, 16, 8, 7);
VAR_FN(8, 8, 8, 6);
VAR_FN(16, 4, 16, 6);
```
