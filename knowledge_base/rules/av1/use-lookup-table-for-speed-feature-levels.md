---
name: libaom/av1/use-lookup-table-for-speed-feature-levels
description: When mapping multi-level speed features to encoder parameters or thresholds, use static const lookup tables with per-level comments and bounds assertions instead of fragmented conditionals
rubric: readability
severity: should_fix
---

**Rule.** When mapping discrete multi-level integer speed features (such as
pruning aggressiveness levels, search methods, or thresholds) to encoder
operating parameters or frame counts, prefer using a static const lookup table
indexed directly by the speed feature level rather than scattered conditional
branches, if/else ladders, or ternary expressions. Document the mapping for each
discrete speed feature level in comments above the table (e.g., `sf_level = 0 =>
...`), and assert that the feature value is within valid lookup bounds before
indexing.

**Why.** Scattering conditional checks and ternary operators across early
returns and calculation paths obscures the mapping between speed feature levels
and their resulting parameters, making it error-prone to audit, tune, or add new
levels. Centralizing the mapping in a static lookup table with level-by-level
documentation clarifies parameter mappings across all intermediate levels,
centralizes tuning values, and prevents subtle out-of-bounds indexing bugs
through assertions.

## Examples

Bad:

```cpp
// Fragmented conditions and ternaries mapping speed feature levels:
if (prune_single_ref == 0 || prune_single_ref > 2 ||
    frame_is_intra_only(cm)) {
  cpi->keep_single_ref_frame_mask =
      (prune_single_ref == 0) ? ((1 << REF_FRAMES) - 1) : 0;
  return;
}
...
const int num_frames_to_keep = (prune_single_ref == 1) ? 5 : 3;
```

Good:

```cpp
// Decide the number of reference frames for which pruning via the speed
// feature prune_single_ref is disallowed:
// prune_single_ref = 0 => None of the 7 reference frames are pruned.
// prune_single_ref = 1 => The best 5 reference frames are not pruned.
// prune_single_ref = 2 => The best 3 reference frames are not pruned.
// prune_single_ref = 3, 4 => All the 7 references are allowed to be pruned.
static const int num_frames_to_keep_lookup[5] = { INTER_REFS_PER_FRAME, 5, 3,
                                                  0, 0 };
assert(prune_single_ref >= 0 && prune_single_ref <= 4);
const int num_frames_to_keep = num_frames_to_keep_lookup[prune_single_ref];
```
