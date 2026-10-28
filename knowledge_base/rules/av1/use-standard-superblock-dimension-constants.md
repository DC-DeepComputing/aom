---
name: libaom/av1/use-standard-superblock-dimension-constants
description: When sizing buffers or calculating grid bounds for worst-case superblock configurations, derive dimensions from standard constants like MIN_MIB_SIZE_LOG2 and use explicit min_/max_ variable prefixes.
rubric: readability
severity: should_fix
---

**Rule.** When sizing buffers or calculating grid dimensions for worst-case,
minimum, maximum, or unconfigured superblock sizes, derive dimensions from
standard superblock and MI dimension constants (such as MIN_MIB_SIZE_LOG2,
MAX_MIB_SIZE, MIN_SB_SIZE_LOG2, or MAX_SB_SIZE) rather than hardcoding block
size enums (such as BLOCK_64X64 or BLOCK_128X128) and indexing mi_size_wide /
mi_size_high. Additionally, explicitly prefix dimension and count variables with
min_ and max_ (such as min_mi_w_sb, min_mi_h_sb, max_sb_cols, max_sb_rows, or
max_sb_square) rather than using generic names (like w, h, num_cols, or
num_rows) that obscure worst-case sizing intent.

**Why.** Deriving grid dimensions from standard superblock constants clearly
communicates that calculations size for worst-case bounds under variable
superblock configurations, avoiding brittle lookup table indexing on arbitrary
block size enums. Explicit min_ and max_ variable prefixes clarify buffer
boundaries and prevent confusion with active frame-level grid parameters.

## Examples

Bad:

```cpp
// Sizing worst-case superblock scaling buffer before actual superblock size is set
const int bsize = BLOCK_64X64;
const int w = mi_size_wide[bsize];
const int h = mi_size_high[bsize];
const int num_cols = (cm->mi_params.mi_cols + w - 1) / w;
const int num_rows = (cm->mi_params.mi_rows + h - 1) / h;
CHECK_MEM_ERROR(cm, cpi->sm_scaling_factor,
                (double *)aom_calloc(num_rows * num_cols,
                                     sizeof(*cpi->sm_scaling_factor)));
```

Good:

```cpp
// Sizing worst-case superblock scaling buffer before actual superblock size is set
const int min_mi_w_sb = (1 << MIN_MIB_SIZE_LOG2);
const int min_mi_h_sb = (1 << MIN_MIB_SIZE_LOG2);
const int max_sb_cols =
    (cm->mi_params.mi_cols + min_mi_w_sb - 1) / min_mi_w_sb;
const int max_sb_rows =
    (cm->mi_params.mi_rows + min_mi_h_sb - 1) / min_mi_h_sb;
CHECK_MEM_ERROR(cm, cpi->sm_scaling_factor,
                (double *)aom_calloc(max_sb_rows * max_sb_cols,
                                     sizeof(*cpi->sm_scaling_factor)));
```
