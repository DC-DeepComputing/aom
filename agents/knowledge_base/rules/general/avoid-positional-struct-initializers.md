---
name: libaom/avoid-positional-struct-initializers
description: When initializing multi-field structs or argument structs with multiple scalar or sentinel values, use an inline init helper instead of positional aggregate initializers
rubric: readability
severity: nit
---

**Rule.** Avoid positional aggregate initializers for structs with multiple
fields or non-obvious literal/sentinel values. Prefer a dedicated inline
initialization helper function (e.g., init_<struct_name>()) or explicit field
assignments to ensure field mappings are clear and maintainable.

**Why.** Positional aggregate initializers omit field names, making scalar
values, sentinels, and boolean flags difficult to understand without
cross-referencing the struct definition. They are also fragile when struct
fields are reordered, inserted, or modified across multiple call sites.

## Examples

Bad:

```cpp
struct estimate_block_intra_args args = { cpi, x,        DC_PRED, 1,
                                          0,   UINT_MAX, false };
```

Good:

```cpp
static INLINE void init_estimate_block_intra_args(
    AV1_COMP *cpi, MACROBLOCK *x,
    struct estimate_block_intra_args *const args) {
  args->cpi = cpi;
  args->x = x;
  args->mode = DC_PRED;
  args->skippable = 1;
  args->rdc = 0;
  args->best_sad = UINT_MAX;
  args->prune_mode_based_on_sad = false;
}

struct estimate_block_intra_args args;
init_estimate_block_intra_args(cpi, x, &args);
```
