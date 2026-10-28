---
name: libaom/document-bitfield-layout-and-combinations-in-enum-flags
description: When defining enum flags or constants encoding bitfield partitions, document the bit allocation, numerical subfield ranges (e.g. 0..15), and valid bitwise combinations
rubric: readability
severity: should_fix
---

**Rule.** When defining enum values, bitmask constants, or bit-packed fields
that encode subfield partitions, reserved bits, or non-contiguous flags,
document the bit layout, valid bitwise combinations, and the explicit numerical
range of allowable values for multi-bit subfields (e.g., `(in the range 0..15)`
for 4 bits). Clearly specify which values are mutually exclusive versus which
can be combined with bitwise OR operations.

**Why.** When flags encode multi-bit subfields or non-contiguous bit positions,
users cannot easily infer valid integer value bounds or which flags can be
safely combined. Explicitly documenting bit allocations, numerical ranges, and
combinability prevents out-of-range subfield values and invalid bitwise
combinations.

## Examples

Bad:

```cpp
/**
 * Currently, the first 4 bits are reserved for the metadata location, and
 * the 5th bit is used to tell if it's layer-specific.
 */
typedef enum aom_metadata_insert_flags {
  AOM_MIF_NON_KEY_FRAME = 0,
  AOM_MIF_KEY_FRAME = 1,
  AOM_MIF_ANY_FRAME = 2,
  AOM_MIF_LAYER_SPECIFIC = 0x10
} aom_metadata_insert_flags_t;
```

Good:

```cpp
/**
 * Currently, the first 4 bits are reserved for the metadata location (in the
 * range 0..15), and the 5th bit is used to tell if it's layer-specific.
 *
 * Only one of the metadata location values should be specified (one of
 * AOM_MIF_NON_KEY_FRAME, AOM_MIF_KEY_FRAME, AOM_MIF_ANY_FRAME).
 * It can be combined with AOM_MIF_LAYER_SPECIFIC with a binary 'or', e.g.:
 * (aom_metadata_insert_flags)(AOM_MIF_KEY_FRAME | AOM_MIF_LAYER_SPECIFIC).
 */
typedef enum aom_metadata_insert_flags {
  AOM_MIF_NON_KEY_FRAME = 0,
  AOM_MIF_KEY_FRAME = 1,
  AOM_MIF_ANY_FRAME = 2,
  AOM_MIF_LAYER_SPECIFIC = 0x10
} aom_metadata_insert_flags_t;
```
