---
name: libaom/keep-error-messages-generic-in-shared-helpers
description: In error messages for shared helpers or dispatched functions, use generic descriptions or function names instead of caller resources or architecture labels like SIMD
rubric: readability
severity: nit
---

**Rule.** In error messages (e.g. via aom_internal_error, aom_codec_set_error,
or diagnostic helpers), do not hardcode caller-specific resource names or
implementation variants and hardware targets (such as 'SIMD', 'AVX2', or
'NEON'). In shared helpers and dispatched functions (such as RTCD functions),
use generic resource descriptions or reference the base function/module name so
messages remain accurate across all call sites, build configurations, and C
fallback executions.

**Why.** Shared helpers operate on multiple caller resources (e.g., source vs.
reference buffers), and dispatched functions can resolve to portable C fallback
implementations or various SIMD targets depending on runtime CPU detection and
build flags. Referencing specific caller resources or 'SIMD' labels produces
misleading diagnostics when invoked from other call paths or when C fallbacks
encounter errors.

## Examples

Bad:

```cpp
// Hardcoding caller resource in shared helper:
if (aom_realloc_frame_buffer(scaled, ...))
  aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                     "Failed to allocate scaled source buffer");

// Hardcoding implementation/architecture label in dispatched function:
if (av1_selfguided_restoration(
        dat8_row + j, w, h, dat_stride, flt0_row + j, flt1_row + j,
        flt_stride, sgr_params_idx, bit_depth, use_highbd) != 0) {
  aom_internal_error(
      error_info, AOM_CODEC_MEM_ERROR,
      "Error allocating buffer in selfguided restoration SIMD function");
}
```

Good:

```cpp
// Generic resource name in shared helper:
if (aom_realloc_frame_buffer(scaled, ...))
  aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                     "Failed to allocate scaled buffer");

// Generic function name regardless of SIMD/C fallback path:
if (av1_selfguided_restoration(
        dat8_row + j, w, h, dat_stride, flt0_row + j, flt1_row + j,
        flt_stride, sgr_params_idx, bit_depth, use_highbd) != 0) {
  aom_internal_error(
      error_info, AOM_CODEC_MEM_ERROR,
      "Error allocating buffer in av1_selfguided_restoration");
}
```
