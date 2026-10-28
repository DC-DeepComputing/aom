---
name: libaom/place-comments-inside-preprocessor-guards
description: When documenting conditionally compiled code or feature blocks, place explanatory comments inside the preprocessor guards rather than outside them
rubric: readability
severity: nit
---

**Rule.** When adding or updating comments that describe conditionally compiled
functions, feature blocks, or code paths, place the explanatory comments inside
the `#if` / `#endif` (or `#ifdef`) preprocessor guards rather than outside them.
Ensure comments are scoped to the code they describe so they do not become
orphaned when the feature macro is disabled.

**Why.** Placing comments describing a conditional feature outside its
preprocessor block leaves orphaned and misleading comments in builds where the
feature macro is disabled. Keeping comments inside the guard ensures they remain
coupled to the code they document and are removed or updated together with the
conditional block.

## Examples

Bad:

```cpp
// Write partition tree to file. Not used by default.
#if COLLECT_MOTION_SEARCH_FEATURE_SB
write_partition_tree(cpi, pc_tree, bsize, mi_row, mi_col);
++cpi->sb_counter;
#endif  // COLLECT_MOTION_SEARCH_FEATURE_SB
```

Good:

```cpp
#if COLLECT_MOTION_SEARCH_FEATURE_SB
// Write partition tree to file. Not used by default.
write_partition_tree(cpi, pc_tree, bsize, mi_row, mi_col);
++cpi->sb_counter;
#endif  // COLLECT_MOTION_SEARCH_FEATURE_SB
```
