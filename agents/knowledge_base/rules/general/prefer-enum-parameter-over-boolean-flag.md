---
name: libaom/prefer-enum-parameter-over-boolean-flag
description: When a function's behavior depends on an operational mode or enum state, pass the enum type directly instead of a boolean flag parameter
rubric: readability
severity: nit
---

**Rule.** When a function's behavior depends on a specific mode, search type, or
variant represented by an enum, pass the enum type directly as a parameter
rather than a derived boolean flag (or boolean equality check). Keep
mode-dependent dispatch and variant handling inside the callee rather than
forcing callers to evaluate boolean conditions or pass bare true/false literals.

**Why.** Passing boolean flags derived from enum comparisons obscures intent at
call sites, forces callers to duplicate enum checks or pass ambiguous true/false
literals, and breaks extensibility when additional modes or enum variants are
introduced.

## Examples

Bad:

```cpp
// Function signature uses a boolean flag derived from an enum mode:
void av1_free_pc_tree_recursive(PC_TREE *tree, int num_planes, int keep_best,
                                int keep_none, bool is_var_based_partition);

// Callers must evaluate enum condition or pass boolean literals:
av1_free_pc_tree_recursive(pc_root, num_planes, 0, 0, true);
av1_free_pc_tree_recursive(pc_root, num_planes, 0, 0,
                           sf->part_sf.partition_search_type == VAR_BASED_PARTITION);
```

Good:

```cpp
// Function signature accepts the enum type directly:
void av1_free_pc_tree_recursive(PC_TREE *tree, int num_planes, int keep_best,
                                int keep_none,
                                PARTITION_SEARCH_TYPE partition_search_type);

// Callers pass the enum value directly, making call sites self-documenting:
av1_free_pc_tree_recursive(pc_root, num_planes, 0, 0, SEARCH_PARTITION);
av1_free_pc_tree_recursive(pc_root, num_planes, 0, 0,
                           sf->part_sf.partition_search_type);
```
