---
name: libaom/initialize-struct-fields-in-declaration-order
description: When initializing struct fields via assignments, setters, or aggregate initializers, or modifying struct definitions, order field initializations to match struct declaration order
rubric: readability
severity: should_fix
---

**Rule.** When initializing or configuring fields of a struct—such as encoder
speed features (e.g., sf->part_sf, sf->rd_sf), configuration parameters
(ConvolveParams, oxcf), or general structs—order all sequential field
assignments and positional aggregate initializers to match the exact declaration
order in the struct definition header. Do not assign or list struct members out
of declaration order. Additionally, when reordering, adding, or modifying fields
in a struct definition, update all corresponding positional aggregate
initializers ({ ... }) across all call sites to match the updated member
declaration order.

**Why.** Matching the struct's declaration order makes it straightforward to
verify completeness against header definitions during code reviews, track
differences across presets, and prevent omissions, duplicate assignments, or
uninitialized fields. For positional aggregate initializers, strict declaration
ordering prevents values from silently initializing the wrong fields or
triggering unintended implicit conversions when struct definitions are modified.

## Examples

Bad:

```cpp
// 1. Encoder speed features assigned out of declaration order from speed_features.h
sf->part_sf.prune_rect_part_using_4x4_var_deviation = true;
sf->part_sf.prune_sub_8x8_partition_level = allow_screen_content_tools ? 0 : 1;
sf->part_sf.prune_part4_search = 3;
sf->part_sf.default_max_partition_size = BLOCK_32X32;
sf->part_sf.prune_rect_part_using_none_pred_mode = true;

// 2. Struct member assignments out of header declaration order
ConvolveParams conv_params;
conv_params.use_dist_wtd_comp_avg = 0;
conv_params.round_0 = WIENER_ROUND0_BITS;
conv_params.dst = NULL;
conv_params.dst_stride = 0;

// 3. Aggregate initialization with mismatched member declaration order
// Struct definition: { bool sign_flag; uint8_t exponent; uint8_t mantissa_len; uint32_t mantissa; }
*element = { sign, static_cast<uint8_t>(exp + 31), mantissa, mantissa_len };
```

Good:

```cpp
// 1. Encoder speed features assigned in exact declaration order from speed_features.h
sf->part_sf.prune_rect_part_using_4x4_var_deviation = true;
sf->part_sf.prune_rect_part_using_none_pred_mode = true;
sf->part_sf.prune_sub_8x8_partition_level = allow_screen_content_tools ? 0 : 1;
sf->part_sf.prune_part4_search = 3;
sf->part_sf.default_max_partition_size = BLOCK_32X32;

// 2. Struct fields initialized following struct declaration order
ConvolveParams conv_params = {
  0, NULL, 0, WIENER_ROUND0_BITS, 2 * FILTER_BITS - WIENER_ROUND0_BITS, 0, 0,
  0, 0,    0
};

// 3. Aggregate initialization matching struct declaration order
// Struct definition: { bool sign_flag; uint8_t exponent; uint8_t mantissa_len; uint32_t mantissa; }
*element = { sign, static_cast<uint8_t>(exp + 31), mantissa_len, mantissa };
```
