---
name: libaom/consistent-compounding-in-related-identifiers
description: When naming related identifiers across structs, functions, and parameters, maintain consistent compounding and word segmentation (e.g., postencode vs post_encode, intrabc vs intra_bc).
rubric: readability
severity: nit
---

**Rule.** Maintain consistent spelling, compounding, and snake_case word
segmentation (e.g., consistently single-word or underscore-separated, such as
postencode vs. post_encode or intrabc vs. intra_bc) for compound domain terms
across related identifiers, including struct members, function names, parameter
lists, and enum constants. When an interface or subsystem establishes a
compounding or abbreviation convention, adhere to that spelling across all
paired functions, members, and helper prototypes.

**Why.** Mixing single-word and underscore-separated spellings or inconsistent
word segmentation across related functions, struct fields, and parameter
signatures creates confusion, increases cognitive load, and makes symbols
difficult to search, grep, or cross-reference across the codebase.

## Examples

Bad:

```cpp
// Inconsistent compounding: mixing post_encode and postencode
if (cpi->rc.post_encode_drop && allow_postencode_drop_rtc(cpi) &&
    av1_post_encode_drop_cbr(cpi, size)) {
  av1_rc_postencode_update_drop_frame(cpi);
}

// Inconsistent word segmentation (intrabc vs intra_bc) across related prototypes:
void av1_hash_table_init(IntraBCHashInfo *intra_bc_hash_info);
void av1_get_block_hash_value(IntraBCHashInfo *intrabc_hash_info, ...);
```

Good:

```cpp
// Consistent compounding: postencode used across all related identifiers
if (cpi->rc.postencode_drop && allow_postencode_drop_rtc(cpi) &&
    av1_postencode_drop_cbr(cpi, size)) {
  av1_rc_postencode_update_drop_frame(cpi);
}

// Consistent word segmentation across all related prototypes:
void av1_hash_table_init(IntraBCHashInfo *intra_bc_hash_info);
void av1_get_block_hash_value(IntraBCHashInfo *intra_bc_hash_info, ...);
```
