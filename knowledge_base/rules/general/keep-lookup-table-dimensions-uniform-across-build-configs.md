---
name: libaom/keep-lookup-table-dimensions-uniform-across-build-configs
description: When defining lookup tables or mode-count constants across compile configurations (#if CONFIG_...), keep array dimensions uniform by duplicating shared entries rather than conditionally resizing tables
rubric: design
severity: should_fix
---

**Rule.** Keep lookup table dimensions and mode-count constants uniform across
build configurations (#if CONFIG_...) by duplicating shared entries or providing
fallback mappings. Avoid conditionally resizing table arrays or changing
dimension macros across compile flags, which forces caller functions and
indexing helpers to introduce #ifdef branches.

**Why.** Varying table dimensions and capacity macros across build
configurations requires conditional #ifdef branching in accessor helpers, loop
bounds, and indexing logic. Maintaining uniform dimensions across configurations
keeps accessors and iteration clean, consistent, and branch-free while avoiding
out-of-bounds indexing in shared code paths.

## Examples

Bad:

```cpp
#if CONFIG_LGT
#define MODE_DEP_INTER_TX_MODES 1
static const int32_t *mdt_mtx_inter_arr4[MODE_DEP_INTER_TX_MODES] = {
  klt4_inter
};
#else
#define MODE_DEP_INTER_TX_MODES 2
static const int32_t *mdt_mtx_inter_arr4[MODE_DEP_INTER_TX_MODES] = {
  single_klt4_inter, comp_klt4_inter
};
#endif
```

Good:

```cpp
#define MODE_DEP_INTER_TX_MODES 2

#if CONFIG_LGT
static const int32_t *mdt_mtx_inter_arr4[MODE_DEP_INTER_TX_MODES] = {
  lgt_klt4_inter, lgt_klt4_inter
};
#else
static const int32_t *mdt_mtx_inter_arr4[MODE_DEP_INTER_TX_MODES] = {
  single_klt4_inter, comp_klt4_inter
};
#endif
```
