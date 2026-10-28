---
name: libaom/combine-identical-branches-with-logical-or
description: When conditional branches execute identical statements or bodies, combine consecutive if and else-if conditions with logical OR (||) or eliminate redundant if-else branches
rubric: readability
severity: should_fix
---

**Rule.** When consecutive if and else-if branches execute identical statement
bodies, blocks, or assignments, combine their conditions into a single branch
using logical OR (`||`). If both branches of an if/else construct execute
identical logic regardless of the condition, remove the redundant conditional
check entirely and execute the consolidated code unconditionally.

**Why.** Duplicating identical code blocks across separate branches introduces
redundant code, increases cyclomatic complexity, triggers static analysis
warnings, and creates maintenance hazards if one branch is modified while the
duplicate is overlooked. Merging branches with logical OR or removing redundant
if-else checks keeps control flow clean and maintainable.

## Examples

Bad:

```cpp
// Bad: Consecutive else-if branches executing identical assignments
if (sb_size == 4 && eob > IST_4x4_HEIGHT - 1) {
  ist_eob = 0;
} else if (sb_size == 8 && eob > IST_8x8_HEIGHT - 1) {
  ist_eob = 0;
}

// Bad: Redundant if-else construct executing identical logic in both branches
if (is_one_pass_rt_params(cpi) &&
    cpi->svc.spatial_layer_id == cpi->svc.number_spatial_layers - 1) {
  ++cpi->gf_frame_index;
  if (cpi->gf_frame_index == MAX_STATIC_GF_GROUP_LENGTH)
    cpi->gf_frame_index = 0;
} else {
  ++cpi->gf_frame_index;
  if (cpi->gf_frame_index == MAX_STATIC_GF_GROUP_LENGTH)
    cpi->gf_frame_index = 0;
}
```

Good:

```cpp
// Good: Combine conditions into a single branch using logical OR (||)
if ((sb_size == 4 && eob > IST_4x4_HEIGHT - 1) ||
    (sb_size == 8 && eob > IST_8x8_HEIGHT - 1)) {
  ist_eob = 0;
}

// Good: Remove redundant conditional and execute consolidated logic directly
++cpi->gf_frame_index;
if (cpi->gf_frame_index == MAX_STATIC_GF_GROUP_LENGTH)
  cpi->gf_frame_index = 0;
```
