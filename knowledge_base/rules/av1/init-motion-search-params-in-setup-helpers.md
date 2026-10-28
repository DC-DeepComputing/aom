---
name: libaom/av1/init-motion-search-params-in-setup-helpers
description: When configuring motion search parameters or function pointers, initialize them in setup helpers like av1_make_default_fullpel_ms_params rather than inside search routines
rubric: design
severity: should_fix
---

**Rule.** Configure motion search parameters and select function pointers (e.g.,
downsampled SAD vs. standard SAD) inside parameter initialization helpers such
as av1_make_default_fullpel_ms_params() or av1_make_default_subpel_ms_params()
rather than inside motion search execution routines like
av1_full_pixel_search(). Keep search routines focused on executing the search
algorithm with const motion search parameter pointers.

**Why.** Decoupling speed-feature evaluation and function-pointer selection from
search execution encapsulates configuration logic in setup helpers, avoids
mutating search parameters during execution, and allows search routines to
accept immutable const parameter pointers.

## Examples

Bad:

```cpp
int av1_full_pixel_search(const FULLPEL_MV start_mv,
                          FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                          const int step_param, int use_downsampled_sad,
                          int *cost_list, FULLPEL_MV *best_mv,
                          FULLPEL_MV *second_best_mv) {
  if (use_downsampled_sad == 1 && block_size_high[bsize] >= 16) {
    ms_params->sdf = ms_params->vfp->sdsf;
    ms_params->sdx4df = ms_params->vfp->sdsx4df;
    ms_params->sdx3df = ms_params->vfp->sdsx4df;
  }
  // ... execute search ...
}
```

Good:

```cpp
void av1_make_default_fullpel_ms_params(
    FULLPEL_MOTION_SEARCH_PARAMS *ms_params, const struct AV1_COMP *cpi,
    MACROBLOCK *x, BLOCK_SIZE bsize, const MV *ref_mv, FULLPEL_MV start_mv,
    const search_site_config search_sites[NUM_DISTINCT_SEARCH_METHODS],
    int fine_search_interval) {
  // ... initialize default search params ...
  if (mv_sf->use_downsampled_sad == 1 && block_size_high[bsize] >= 16 &&
      !is_key_frame) {
    ms_params->sdf = ms_params->vfp->sdsf;
    ms_params->sdx4df = ms_params->vfp->sdsx4df;
    ms_params->sdx3df = ms_params->vfp->sdsx4df;
  }
}

int av1_full_pixel_search(const FULLPEL_MV start_mv,
                          const FULLPEL_MOTION_SEARCH_PARAMS *ms_params,
                          const int step_param, int *cost_list,
                          FULLPEL_MV *best_mv, FULLPEL_MV *second_best_mv);
```
