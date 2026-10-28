---
name: libaom/consolidate-variable-computation-at-declaration
description: When computing mode- or feature-level-dependent local variables, explicitly structure all derivation branches at the declaration site instead of initializing speculative defaults and overriding downstream
rubric: readability
severity: should_fix
---

**Rule.** Consolidate local variable computation and mode- or level-dependent
derivation logic into explicit branches at or immediately following the
variable's declaration. Do not assign an implicit or speculative default value
for one mode or level and scatter conditional overrides across downstream
blocks; explicitly handle each configuration mode or feature level in distinct
`if`/`else` branches.

**Why.** Scattering variable mutations across downstream conditional blocks or
relying on implicit default initializations that get overridden obscures the
variable's derivation, makes mode- and level-specific logic harder to trace, and
increases the risk of unintended state leaks.

## Examples

Bad:

```cpp
// Speculative initialization representing one level/mode with conditional overrides
int prune_sub_8x8 = 1;
if (cpi->sf.part_sf.prune_sub_8x8_partition_level == 1) {
  if (!xd->left_available || !xd->up_available || num_neighbors_lt_8x8 == 2) {
    prune_sub_8x8 = 0;
  }
}

int max_delta_down = AOMMIN(16, AOMMAX(1, rc->q_1_frame / 8));
if (use_cyclic_refresh && cpi->is_screen_content_type) {
  if (cr->sb_index > cr->last_sb_index) {
    max_delta_down = AOMMIN(8, AOMMAX(1, rc->q_1_frame / 32));
  }
}
```

Good:

```cpp
// Explicit branching for each feature level and consolidated mode branches
int prune_sub_8x8;
if (cpi->sf.part_sf.prune_sub_8x8_partition_level == 2) {
  prune_sub_8x8 = 1;
} else {
  assert(cpi->sf.part_sf.prune_sub_8x8_partition_level == 1);
  prune_sub_8x8 = xd->left_available && xd->up_available && (num_neighbors_lt_8x8 < 2);
}

int max_delta_down;
if (aq_mode == CYCLIC_REFRESH_AQ && apply_cyclic_refresh) {
  max_delta_down = is_screen && (sb_index > last_sb_index)
                       ? AOMMIN(8, AOMMAX(1, q_1_frame / 32))
                       : AOMMIN(16, AOMMAX(1, q_1_frame / 8));
} else {
  max_delta_down = is_screen ? AOMMIN(8, AOMMAX(1, q_1_frame / 16))
                             : AOMMIN(16, AOMMAX(1, q_1_frame / 8));
}
```
