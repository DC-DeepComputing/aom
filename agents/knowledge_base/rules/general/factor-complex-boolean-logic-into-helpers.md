---
name: libaom/factor-complex-boolean-logic-into-helpers
description: When computing complex multi-step boolean flags or gating conditions, extract the decision logic into a dedicated helper function with early guard returns
rubric: design
severity: should_fix
---

**Rule.** Factor complex multi-step boolean calculations, gating conditions, and
flag decision heuristics into dedicated helper functions with early guard
returns rather than mutating and chaining boolean expressions across multiple
statements inline. Structure the helper function with guard clauses that return
false early before evaluating remaining positive conditions.

**Why.** Sequentially mutating and reassigning boolean flag variables across
multiple statements clutters the caller scope, introduces mutable local state,
and obscures individual gating preconditions. Extracting the decision into a
dedicated inline helper function with early guard returns isolates the
heuristic, clarifies each prerequisite check, and simplifies maintenance.

## Examples

Bad:

```cpp
int try_palette =
    !skip_idtx_palette && cpi->oxcf.tool_cfg.enable_palette &&
    av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                      mi->bsize);
try_palette =
    try_palette &&
    (is_mode_intra(best_pickmode->best_mode) || force_palette_test) &&
    x->source_variance > 0 && !x->force_zeromv_skip_for_blk &&
    (cpi->rc.high_source_sad || x->source_variance > 300);

if (rt_sf->prune_palette_search_nonrd > 1 &&
    ((cpi->rc.high_source_sad && cpi->ppi->rtc_ref.non_reference_frame) ||
     bsize > BLOCK_16X16)) {
  try_palette = 0;
}
```

Good:

```cpp
static AOM_INLINE bool enable_palette(const AV1_COMP *cpi, bool is_mode_intra,
                                      BLOCK_SIZE bsize,
                                      unsigned int source_variance,
                                      int force_zeromv_skip,
                                      int skip_idtx_palette,
                                      int force_palette_test) {
  if (!cpi->oxcf.tool_cfg.enable_palette) return false;
  if (!av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                         bsize)) {
    return false;
  }
  if (skip_idtx_palette) return false;

  if (cpi->sf.rt_sf.prune_palette_search_nonrd > 1 &&
      ((cpi->rc.high_source_sad && cpi->ppi->rtc_ref.non_reference_frame) ||
       bsize > BLOCK_16X16)) {
    return false;
  }

  return (is_mode_intra || force_palette_test) && source_variance > 0 &&
         !force_zeromv_skip &&
         (cpi->rc.high_source_sad || source_variance > 300);
}
```
