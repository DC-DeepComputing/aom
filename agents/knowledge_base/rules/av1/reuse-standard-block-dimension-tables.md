---
name: libaom/av1/reuse-standard-block-dimension-tables
description: When querying block or transform dimensions from BLOCK_SIZE or TX_SIZE, reuse standard lookup tables from common_data.h instead of defining redundant local arrays
rubric: readability
severity: should_fix
---

**Rule.** When querying mode-info units, pixel dimensions, or log2 sizes for a
given BLOCK_SIZE or TX_SIZE, reuse the standard lookup tables defined in
av1/common/common_data.h (such as mi_size_wide_log2, mi_size_high_log2,
block_size_wide, block_size_high, tx_size_wide, or tx_size_high). Do not declare
redundant local lookup arrays for standard block or transform properties.

**Why.** Defining duplicate block or transform dimension tables increases code
duplication and maintenance burden, and risks subtle divergence from standard
AV1 mode-info and pixel geometry definitions.

## Examples

Bad:

```cpp
// Redundant local lookup tables for block width and height log2 sizes
static const uint8_t b_width_log2_lookup[BLOCK_SIZES] = { 0, 0, 1, 1, 1, 2, ... };
static const uint8_t b_height_log2_lookup[BLOCK_SIZES] = { 0, 1, 0, 1, 2, 1, ... };

uint32_t norm_color_dist =
    x->min_dist_inter_uv >> (b_width_log2_lookup[bsize] + b_height_log2_lookup[bsize]);
```

Good:

```cpp
// Reuse standardized lookup tables from av1/common/common_data.h
uint32_t norm_color_dist =
    x->min_dist_inter_uv >> (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]);
```
