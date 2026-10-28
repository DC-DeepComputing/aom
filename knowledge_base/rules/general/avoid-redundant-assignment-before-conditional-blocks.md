---
name: libaom/avoid-redundant-assignment-before-conditional-blocks
description: When temporarily modifying and restoring a variable or struct member around a conditional block, assign its baseline value once after the block instead of beforehand
rubric: readability
severity: nit
---

**Rule.** Do not perform dead stores or redundant variable or field assignments
immediately before a conditional block when the value is temporarily overwritten
inside the block and must hold a common baseline value across all paths
afterwards. Omit the pre-conditional assignment and place the baseline
assignment once after the conditional block instead of duplicating it beforehand
and at the end of the block.

**Why.** Assigning a variable or struct member immediately before an if-block
that unconditionally overwrites it creates an unnecessary dead store and
obscures data flow. Consolidating the final assignment once after the
conditional block ensures all execution paths receive the expected baseline
value without redundant assignments or duplicate logic.

## Examples

Bad:

```cpp
// Redundant pre-assignment creates a dead store when the branch is taken
p_rc->accumulate_stats_stage = (cpi->oxcf.mode == REALTIME);
if (cpi->oxcf.mode != REALTIME) {
  p_rc->accumulate_stats_stage = true;
  // ... temporary processing ...
  p_rc->accumulate_stats_stage = (cpi->oxcf.mode == REALTIME);
}
```

Good:

```cpp
// Assign temporary state inside the branch and set baseline value once after
if (cpi->oxcf.mode != REALTIME) {
  p_rc->accumulate_stats_stage = true;
  // ... temporary processing ...
}
p_rc->accumulate_stats_stage = (cpi->oxcf.mode == REALTIME);
```
