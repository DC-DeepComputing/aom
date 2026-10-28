---
name: libaom/consolidate-duplicate-local-variables
description: When complex conditions, loop boundaries, calculations, bitwise masks, or call arguments are non-trivial or repeated, consolidate them into descriptive const local variables
rubric: readability
severity: should_fix
---

**Rule.** Do not inline complex, multi-clause, or inverted conditions, complex
loop header boundaries (such as ternary expressions, AOMMIN/AOMMAX, or helper
calls in for loops), or duplicate calculations, common subexpressions, bitwise
masks (e.g., reusable mask and ~mask), and function call arguments across
multiple usage sites within the same scope. Extract or consolidate these
expressions into descriptively named const local variables (such as const bool,
const int, plane_start, plane_end, or blk_size) immediately before use. Reuse
existing computed local variables across subsequent checks rather than
re-evaluating equivalent expressions. Additionally, when a function or code path
is invariant to execute only in a specific mode or partition type, explicitly
assert the precondition with assert() and use fixed constant indices rather than
dynamic conditional expressions.

**Why.** Inlining complex conditions, loop boundaries, or compound calculations
clutters control flow, obscures developer intent, hinders debugging, and can
lead to logic inversion or redundant re-evaluation bugs. Duplicating non-trivial
expressions or bitwise operations across branches and call sites creates
copy-paste hazards where logic is updated in one location but forgotten in
another. Extracting expressions into named const local variables makes code
self-documenting, simplifies branching and loop headers, guarantees consistent
evaluation, and clarifies caller contracts.

## Examples

Bad:

```cpp
// Obscure inline condition and repeated boolean argument across call sites
if (!is_screen || bsize < cm->seq_params->sb_size) { ... }
p_rc->gfu_boost = use_alt_ref
    ? av1_calc_arf_boost(..., cpi->oxcf.mode != REALTIME)
    : AOMMIN(MAX_GF_BOOST, av1_calc_arf_boost(..., cpi->oxcf.mode != REALTIME));

// Complex loop boundaries inlined in for-loop header
for (int plane = (xd->tree_type == CHROMA_PART);
     plane < (xd->tree_type == LUMA_PART
                  ? 1
                  : AOMMIN(2, av1_num_planes(&pbi->common)));
     ++plane) { ... }

// Duplicated bitwise mask/index, re-evaluating existing condition, and dynamic indexing
const bool use_ccso = (plane == 1) ? mi_grid[calc_idx(x, y)]->ccso_blk_u
                                   : mi_grid[calc_idx(x, y)]->ccso_blk_v;
const int is_chroma = (xd->tree_type == CHROMA_PART);
if (xd->tree_type == CHROMA_PART && bsize == BLOCK_8X8) return PARTITION_NONE;
const BLOCK_SIZE cur_bsize = mbmi->sb_type[xd->tree_type == CHROMA_PART];
```

Good:

```cpp
// Explanatory const bool and consolidated argument variable
const bool screen_scroll_sb = is_screen && bsize == cm->seq_params->sb_size;
if (!screen_scroll_sb) { ... }
const bool scale_max_boost = (cpi->oxcf.mode != REALTIME);
p_rc->gfu_boost = use_alt_ref
    ? av1_calc_arf_boost(..., scale_max_boost)
    : AOMMIN(MAX_GF_BOOST, av1_calc_arf_boost(..., scale_max_boost));

// Extracted descriptive loop bounds before the loop header
const int plane_start = (xd->tree_type == CHROMA_PART);
const int plane_end = (xd->tree_type == LUMA_PART
                           ? 1
                           : AOMMIN(2, av1_num_planes(&pbi->common)));
for (int plane = plane_start; plane < plane_end; ++plane) { ... }

// Consolidated index/mask, reused local variable, and asserted mode invariant
const int ccso_blk_idx = calc_idx(x, y);
const bool use_ccso = (plane == 1) ? mi_grid[ccso_blk_idx]->ccso_blk_u
                                   : mi_grid[ccso_blk_idx]->ccso_blk_v;
const int is_chroma = (xd->tree_type == CHROMA_PART);
if (is_chroma && bsize == BLOCK_8X8) return PARTITION_NONE;
assert(xd->tree_type != CHROMA_PART);
const BLOCK_SIZE cur_bsize = mbmi->sb_type[0];
```
