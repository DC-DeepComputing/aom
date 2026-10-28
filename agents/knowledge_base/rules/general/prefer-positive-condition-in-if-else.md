---
name: libaom/prefer-positive-condition-in-if-else
description: In two-branch if-else statements and ternary expressions without early exits, prefer positive conditions over negated checks (! or !=) and swap branches
rubric: readability
severity: should_fix
---

**Rule.** In two-branch if-else statements and ternary conditional expressions
(? :) where neither branch is an early exit (such as a return, break, or
continue guard clause), prefer testing positive conditions (e.g., if (cond), if
(x == y), or cond ? a : b) rather than negated conditions (e.g., if (!cond), if
(x != y), or !cond ? b : a). Swap the true and false branch bodies or
expressions so the affirmative case is evaluated and placed first.

**Why.** Affirmative condition checks align with natural cognitive flow and
avoid double negation when reasoning about the alternate (else or false) branch.
Leading logical NOT (!) or negative comparisons (!=) add unnecessary indirection
and mental overhead when both branches are explicitly handled.

## Examples

Bad:

```cpp
// Bad: Negated condition in two-branch if-else statement.
if (!rc_is_valid_) {
  lpf_level.filter_level[0] = 0;
} else {
  av1_pick_filter_level(nullptr, cpi_, LPF_PICK_FROM_Q);
  lpf_level.filter_level[0] = cpi_->common.lf.filter_level[0];
}

// Bad: Inequality comparison in two-branch if-else statement.
if (height != 2) {
  store_u16_8x4(dst, dst_stride, d0, d1, d2, d3);
} else {
  store_u16_8x2(dst, dst_stride, d0, d1);
}

// Bad: Negated condition in ternary expression.
const int tile_idx = !pack_bs_sync->pack_bs_mt_exit
                         ? get_next_pack_bs_tile_idx(pack_bs_sync, num_tiles)
                         : -1;
```

Good:

```cpp
// Good: Positive condition with swapped branches.
if (rc_is_valid_) {
  av1_pick_filter_level(nullptr, cpi_, LPF_PICK_FROM_Q);
  lpf_level.filter_level[0] = cpi_->common.lf.filter_level[0];
} else {
  lpf_level.filter_level[0] = 0;
}

// Good: Equality comparison placed first.
if (height == 2) {
  store_u16_8x2(dst, dst_stride, d0, d1);
} else {
  store_u16_8x4(dst, dst_stride, d0, d1, d2, d3);
}

// Good: Positive condition in ternary expression.
const int tile_idx = pack_bs_sync->pack_bs_mt_exit
                         ? -1
                         : get_next_pack_bs_tile_idx(pack_bs_sync, num_tiles);
```
