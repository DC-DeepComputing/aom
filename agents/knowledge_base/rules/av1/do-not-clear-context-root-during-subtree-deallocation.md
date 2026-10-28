---
name: libaom/av1/do-not-clear-context-root-during-subtree-deallocation
description: When pruning subtrees or executing recursive tree deallocations in encoder search, do not reset context root pointers (like td->pc_root) to NULL until top-level deallocation
rubric: safety
severity: should_fix
---

**Rule.** Do not reset thread context root pointers (such as td->pc_root or
thread_data->td->pc_root) to NULL during recursive or partial tree deallocation
passes. Only clear context root pointers when the entire root node and all child
subtrees are completely freed at top-level superblock or frame scope.

**Why.** In recursive partition search, sub-block passes only prune non-selected
partition branches while keeping the best nodes alive, and local pc_tree
pointers often reference child subtree nodes rather than the tree root.
Resetting td->pc_root to NULL during partial subtree pruning breaks lifetime
tracking required for early-termination cleanup and leaves context pointers
inconsistent while the partition tree remains active.

## Examples

Bad:

```cpp
if (pc_tree_dealloc == 0) {
  av1_free_pc_tree_recursive(pc_tree, num_planes, 1, 1,
                             cpi->sf.part_sf.partition_search_type);
}
td->pc_root = NULL;
```

Good:

```cpp
if (pc_tree_dealloc == 0) {
  av1_free_pc_tree_recursive(pc_tree, num_planes, 1, 1,
                             cpi->sf.part_sf.partition_search_type);
}
```
