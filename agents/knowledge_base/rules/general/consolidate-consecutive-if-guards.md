---
name: libaom/consolidate-consecutive-if-guards
description: Consolidate nested or shared if guards with &&, parenthesize sub-expressions, eliminate redundant sub-conditions subsumed by preceding guards or invariants, and extract large guarded blocks into static helpers
rubric: readability
severity: should_fix
---

**Rule.** Combine directly or sequentially nested `if` statements that have no
intervening statements, intermediate variable declarations, or `else` clauses
into a single compound `if` statement using logical AND (`&&`). Explicitly wrap
logical OR (`||`) sub-expressions in parentheses to ensure clear and unambiguous
operator precedence.

Merge adjacent or consecutive `if` statements that share an identical condition into a single conditional block, and factor common prerequisite checks from sibling branches into an enclosing outer `if` guard.

Eliminate redundant sub-conditions or outer checks that are already subsumed by preceding early-return guards, outer conditions, intermediate boolean flags, or initialization invariants (such as non-zero dimension guarantees). Avoid repeating condition checks across conjunction levels (e.g., simplify `((A && C) || B) && C` to `((A || B) && C)`) and omit relational bounds checks logically implied by validated preconditions (e.g., simplify `x >= y && x > 0` to `x >= y` when `y >= 1` is guaranteed).

Additionally, when nested unbranched guards enclose large, self-contained calculation blocks, motion estimation, or tuning sub-algorithms, combine the guard conditions with `&&` and factor the body out into a dedicated `static` helper function rather than nesting deeply inline.

**Why.** Sequentially nesting unbranched `if` statements increases indentation
depth, visual clutter, and cognitive load without changing control-flow
semantics. Combining guards with `&&` flattens control flow, keeps early returns
concise, and leverages short-circuit boolean evaluation. Merging shared
conditions avoids redundant evaluations, while factoring large calculation
blocks into dedicated `static` helpers modularizes sub-algorithms. Eliminating
subsumed or redundant boolean sub-conditions removes dead checks, prevents
misleading control flow, and avoids confusing readers about whether
preconditions or sibling variables could take unexpected boundary values.

## Examples

Bad:

```cpp
// Bad: Nested if guards without intervening statements
if (cpi->oxcf.algo_cfg.sharpness == 3) {
  if (mbmi->motion_mode == OBMC_CAUSAL ||
      mbmi->motion_mode == WARPED_CAUSAL) {
    continue;
  }
}

// Bad: Redundant sub-conditions subsumed by preceding guard or implied by precondition (num_workers >= 1)
if ((blk_row >= num_rows - 1) || (blk_col >= num_cols - 1)) return curr_sad;
if (cm->seq_params->sb_size == BLOCK_128X128 &&
    blk_col + 1 < num_cols && blk_row + 1 < num_rows) {
  process_block(cpi);
}
if (p_mt_info->num_workers >= num_workers && p_mt_info->num_workers > 0) {
  return;
}

// Bad: Repeated conjunct across branches and obsolete non-zero check
if (((*frame_type == KEY_FRAME && cpi->sf.rt_sf.rc_adjust_keyframe &&
      cpi->oxcf.rc_cfg.max_intra_bitrate_pct > 0) ||
     (cpi->sf.rt_sf.rc_compute_spatial_var_sc && rc->high_source_sad)) &&
    cpi->oxcf.rc_cfg.max_intra_bitrate_pct > 0) {
  adjust_frame_rate(cpi);
}

// Bad: Deeply nested guards around complex inline logic
if (est_motion == 1 || est_motion == 2) {
  if (xd->mb_to_right_edge >= 0 && xd->mb_to_bottom_edge >= 0) {
    // 40+ lines of inline motion estimation and tuning logic...
  }
}
```

Good:

```cpp
// Good: Combined into single compound if with logical && and parentheses
if (cpi->oxcf.algo_cfg.sharpness == 3 &&
    (mbmi->motion_mode == OBMC_CAUSAL ||
     mbmi->motion_mode == WARPED_CAUSAL)) {
  continue;
}

// Good: Subsumed and implied sub-conditions omitted
if ((blk_row >= num_rows - 1) || (blk_col >= num_cols - 1)) return curr_sad;
if (cm->seq_params->sb_size == BLOCK_128X128) {
  process_block(cpi);
}
if (p_mt_info->num_workers >= num_workers) {
  return;
}

// Good: Common conjunct factored out and evaluated once
if (((*frame_type == KEY_FRAME && cpi->sf.rt_sf.rc_adjust_keyframe) ||
     (cpi->sf.rt_sf.rc_compute_spatial_var_sc && rc->high_source_sad)) &&
    cpi->oxcf.rc_cfg.max_intra_bitrate_pct > 0) {
  adjust_frame_rate(cpi);
}

// Good: Guard flattened and complex body extracted into static helper
static void do_int_pro_motion_estimation(AV1_COMP *cpi, MACROBLOCK *x, ...) {
  // 40+ lines of motion estimation and tuning logic...
}

if ((est_motion == 1 || est_motion == 2) && xd->mb_to_right_edge >= 0 &&
    xd->mb_to_bottom_edge >= 0) {
  do_int_pro_motion_estimation(cpi, x, ...);
}
```
