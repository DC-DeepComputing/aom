---
name: libaom/update-callers-in-conditionally-compiled-sources
description: When modifying function signatures or parameter lists, verify and update all callers across conditionally compiled sources and non-default build configurations
rubric: correctness
severity: blocking
---

**Rule.** When updating a function declaration, definition, or prototype
signature, verify and update all call sites across all conditionally compiled
files and optional build configurations (such as sources guarded by `#if
CONFIG_*` or enabled only in specific CMake build flags like `CONFIG_TUNE_VMAF`
or `CONFIG_TUNE_BUTTERAUGLI`). Do not rely solely on default build targets when
refactoring shared functions.

**Why.** Sources guarded by non-default build flags or optional features are
omitted during standard local builds. Modifying a function signature without
updating callers in these conditionally compiled modules leads to compilation
errors in downstream CI, nightly builds, or specialized configurations.

## Examples

Bad:

```cpp
// av1/encoder/mcomp.h:
void av1_make_default_fullpel_ms_params(
    FULLPEL_MOTION_SEARCH_PARAMS *ms_params, const struct AV1_COMP *cpi,
    MACROBLOCK *x, BLOCK_SIZE bsize, const MV *ref_mv, FULLPEL_MV start_mv,
    const search_site_config search_sites[NUM_DISTINCT_SEARCH_METHODS],
    int fine_search_interval);

// av1/encoder/tune_vmaf.c (guarded by CONFIG_TUNE_VMAF - missed during refactor):
av1_make_default_fullpel_ms_params(&full_ms_params, cpi, mb, block_size,
                                   &baseline_mv, search_site_cfg,
                                   /*fine_search_interval=*/0);
```

Good:

```cpp
// av1/encoder/mcomp.h:
void av1_make_default_fullpel_ms_params(
    FULLPEL_MOTION_SEARCH_PARAMS *ms_params, const struct AV1_COMP *cpi,
    MACROBLOCK *x, BLOCK_SIZE bsize, const MV *ref_mv, FULLPEL_MV start_mv,
    const search_site_config search_sites[NUM_DISTINCT_SEARCH_METHODS],
    int fine_search_interval);

// av1/encoder/tune_vmaf.c (updated alongside signature change):
av1_make_default_fullpel_ms_params(&full_ms_params, cpi, mb, block_size,
                                   &baseline_mv, start_mv, search_site_cfg,
                                   /*fine_search_interval=*/0);
```
