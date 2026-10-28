---
name: libaom/document-lookup-table-input-output-mapping
description: When defining lookup tables, conversion arrays, or bitmasks, document input-output mappings and bit semantics, and sync explanatory formulas with current codebase helpers
rubric: readability
severity: should_fix
---

**Rule.** When declaring or defining lookup tables, conversion arrays, context
mappings, or bitmask tables (such as static constant arrays), precede the
declaration with comments documenting the input index domain, output semantics,
and table purpose. For bitmask arrays, document what bit positions correspond to
(e.g., mapping to enum types), what setting or clearing a bit signifies, and
under what algorithm modes the table is active. Additionally, ensure any
explanatory pseudo-code or derivation formulas reference current codebase helper
and identifier names rather than legacy or outdated specification names.

**Why.** Lookup tables and bitmask arrays containing raw numeric values are
opaque without clear documentation of their index domains, bit polarities, and
output semantics. Furthermore, formulas ported from older specs or legacy codecs
often retain obsolete helper names. Documenting input-output mappings and
synchronizing derivation formulas with current codebase helpers prevents
indexing bugs, clarifies algorithmic intent, and makes derivations easily
verifiable during maintenance.

## Examples

Bad:

```cpp
// Variant 1: Outdated helper names in derivation formula
// AOMMIN(3, AOMMIN(b_width_log2(bsize), b_height_log2(bsize)))
// The Size_Group table in the spec (Section 9.3. Conversion tables).
static const uint8_t size_group_lookup[BLOCK_SIZES_ALL] = {
  0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3,
};

// Variant 2: Undocumented bitmask table and input/output semantics
static const uint32_t partition_subsize_mask[BLOCK_SIZES_ALL] = {
  0x0001, 0x0002, 0x0004, 0x0008,
};
```

Good:

```cpp
// Variant 1: Clear purpose and current helper identifiers in formula
// Maps a block size to a size-group context.
// Spec: Section 9.3. Conversion tables (Size_Group).
// AOMMIN(3, AOMMIN(mi_size_wide_log2(bsize), mi_size_high_log2(bsize)))
static const uint8_t size_group_lookup[BLOCK_SIZES_ALL] = {
  0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3,
};

// Variant 2: Documented index domain, bit-to-enum mapping, and active conditions
// Maps BLOCK_SIZE index to a bitmask of valid partition split types (1 << PARTITION_TYPE).
// Used during partition search to prune disallowed block subdivisions.
static const uint32_t partition_subsize_mask[BLOCK_SIZES_ALL] = {
  0x0001, 0x0002, 0x0004, 0x0008,
};
```
