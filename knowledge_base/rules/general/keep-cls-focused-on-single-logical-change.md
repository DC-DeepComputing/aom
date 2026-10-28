---
name: libaom/keep-cls-focused-on-single-logical-change
description: When preparing or reviewing a CL, keep changes focused on a single logical purpose and split unrelated edits, optimizations, or refactorings into separate CLs
rubric: change_scope
severity: should_fix
---

**Rule.** Do not bundle unrelated edits, optimizations, or refactorings into a
CL that addresses a specific bug fix or feature. Split independent changes into
separate, focused CLs so each change can be reviewed, benchmarked, and landed on
its own merits.

**Why.** Bundling unrelated changes into a single CL increases review
complexity, risks delaying urgent bug fixes behind unrelated refactorings, and
complicates git bisect investigations when regressions occur.

## Examples

Bad:

```cpp
// In a CL fixing a loop restoration crash, bundling unrelated prediction changes:
void aom_comp_mask_upsampled_pred(...) {
  aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                     subpel_x_q3, subpel_y_q3, ref, ref_stride, subpel_search);
  aom_comp_mask_pred(comp_pred, pred, width, height, comp_pred, width, mask,
                     mask_stride, invert_mask);
}
```

Good:

```cpp
// Keep the CL focused solely on the loop restoration crash fix.
// Submit unrelated prediction logic or refactorings in a separate CL.
```
