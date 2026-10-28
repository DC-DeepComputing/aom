---
name: libaom/zero-initialize-c-structs
description: Zero-initialize C structs at declaration using = { 0 }, av1_zero(), memset, or aom_calloc, and omit redundant member assignments to 0 or NULL
rubric: readability
severity: should_fix
---

**Rule.** When declaring C structs where unused members need default-clearing,
zero-initialize the entire struct upon declaration using `= { 0 };`,
`memset(&var, 0, sizeof(var))`, or the `av1_zero(var)` helper macro (or allocate
dynamic structs with `aom_calloc()`), rather than leaving the struct
uninitialized and manually assigning zero or NULL to individual fields.
Furthermore, do not add redundant member assignments setting fields to `0`,
`NULL`, or `false` when the struct or buffer is already zero-initialized (via `=
{ 0 }`, `av1_zero()`, `memset()`, or `aom_calloc()`), or when values are already
established by an initializer list.

**Why.** Zero-initializing structs with `= { 0 }`, `av1_zero()`, `memset()`, or
`aom_calloc()` guarantees all integer/enum fields are zero, booleans are false,
and pointer fields are NULL. This prevents uninitialized memory defects,
satisfies static analysis tools (e.g. Coverity), and eliminates manual
field-by-field zeroing boilerplate. Re-assigning fields to 0 or NULL after
whole-struct zero-initialization is redundant, adds visual noise, and obscures
intentional non-zero state mutations.

## Examples

Bad:

```cpp
// Bad: Declaring uninitialized struct and manually zeroing fields
ConvolveParams conv_params;
conv_params.do_average = 0;
conv_params.is_compound = 0;
conv_params.round_0 = WIENER_ROUND0_BITS;
conv_params.dst = NULL;
conv_params.dst_stride = 0;

// Bad: Redundantly assigning 0/NULL after aom_calloc or zero-initialization
PC_TREE *pc_tree = aom_calloc(1, sizeof(*pc_tree));
if (pc_tree == NULL) return NULL;
pc_tree->partitioning = PARTITION_NONE;
pc_tree->index = 0;      // Redundant: already 0 from aom_calloc
pc_tree->parent = NULL;  // Redundant: already NULL from aom_calloc
```

Good:

```cpp
// Good: Zero-initialize struct at declaration and assign only non-zero members
ConvolveParams conv_params = { 0 };
conv_params.round_0 = WIENER_ROUND0_BITS;

// Good: Use av1_zero() or memset() where helper macro zeroing is preferred
struct SubgopStats stats;
av1_zero(stats);

// Good: Assign only non-zero members on calloc-allocated struct
PC_TREE *pc_tree = aom_calloc(1, sizeof(*pc_tree));
if (pc_tree == NULL) return NULL;
pc_tree->partitioning = PARTITION_NONE;
```
