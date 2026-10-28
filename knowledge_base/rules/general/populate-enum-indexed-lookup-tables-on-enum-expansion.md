---
name: libaom/populate-enum-indexed-lookup-tables-on-enum-expansion
description: When adding new enum values or feature-gated modes, ensure all lookup tables indexed by that enum are updated with corresponding entries
rubric: correctness
severity: should_fix
---

**Rule.** When extending an enum or adding feature-gated modes (such as new
block sizes, transform sizes, or partition types), update all lookup tables and
context mapping arrays indexed or sized by that enum (e.g., arrays sized by
*_ALL or *_COUNT). Ensure any feature guards (such as #if CONFIG_*) surrounding
new enum values are consistently mirrored in every corresponding lookup table.

**Why.** Lookup tables sized by an enum count or indexed by enum values will
contain uninitialized data, zeroed fallbacks, or induce out-of-bounds reads if
new enum values are added without matching table entries. Mirroring feature
guards and populating all related tables prevents silent logic errors, missing
context mappings, and runtime crashes when new modes are exercised.

## Examples

Bad:

```cpp
// Adding new block sizes under CONFIG_FLEX_PARTITION without populating table entries
static const struct {
  PARTITION_CONTEXT above;
  PARTITION_CONTEXT left;
} partition_context_lookup[BLOCK_SIZES_ALL] = {
  // ... standard block sizes ...
#if CONFIG_FLEX_PARTITION
  // Missing entries for newly added block sizes
#endif  // CONFIG_FLEX_PARTITION
};
```

Good:

```cpp
// All newly added enum modes are explicitly populated in the lookup table
static const struct {
  PARTITION_CONTEXT above;
  PARTITION_CONTEXT left;
} partition_context_lookup[BLOCK_SIZES_ALL] = {
  // ... standard block sizes ...
#if CONFIG_FLEX_PARTITION
  { 31, 24 },  // BLOCK_4X32
  { 24, 31 },  // BLOCK_32X4
  { 30, 16 },  // BLOCK_8X64
  { 16, 30 },  // BLOCK_64X8
  { 31, 16 },  // BLOCK_4X64
  { 16, 31 },  // BLOCK_64X4
#endif  // CONFIG_FLEX_PARTITION
};
```
