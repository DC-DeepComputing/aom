---
name: libaom/use-hex-or-shifts-for-bitmask-and-flag-constants
description: When defining bitmasks, bit-flag constants, or bitfield enum values, use hexadecimal notation or explicit bit shifts rather than decimal literals
rubric: readability
severity: should_fix
---

**Rule.** When defining bitmasks, bit-flag constants, capability masks, or
bitfield enumerator values, format the values using hexadecimal notation (such
as `0x10`) or explicit bit-shift expressions (such as `1 << 4`) rather than
decimal literals (such as `16`). Keep notation consistent across all related
flags in the enum or macro family.

**Why.** Hexadecimal notation (e.g., `0x01`, `0x10`) and bit shifts (e.g., `1 <<
0`, `1 << 4`) make active bit positions and mask patterns immediately
recognizable. Decimal literals obscure the binary structure, making bitwise
operations and composite flag definitions difficult to audit.

## Examples

Bad:

```cpp
// Decimal literals obscure bit positions and masks in flag definitions
typedef enum {
  AOM_MIF_NON_KEY_FRAME = 0,
  AOM_MIF_KEY_FRAME = 1,
  AOM_MIF_ANY_FRAME = 2,
  AOM_MIF_NON_KEY_FRAME_LAYER_SPECIFIC = 16,
  AOM_MIF_KEY_FRAME_LAYER_SPECIFIC = 17,
  AOM_MIF_ANY_FRAME_LAYER_SPECIFIC = 18,
} aom_metadata_insert_flags_t;

#define AOM_FLAG_FEATURE_A 16
#define AOM_FLAG_FEATURE_B 32
```

Good:

```cpp
// Hexadecimal literals or bit shifts make active bits and masks explicit
typedef enum {
  AOM_MIF_NON_KEY_FRAME = 0x00,
  AOM_MIF_KEY_FRAME = 0x01,
  AOM_MIF_ANY_FRAME = 0x02,
  AOM_MIF_NON_KEY_FRAME_LAYER_SPECIFIC = 0x10,
  AOM_MIF_KEY_FRAME_LAYER_SPECIFIC = 0x11,
  AOM_MIF_ANY_FRAME_LAYER_SPECIFIC = 0x12,
} aom_metadata_insert_flags_t;

#define AOM_FLAG_FEATURE_A 0x10  // or (1 << 4)
#define AOM_FLAG_FEATURE_B 0x20  // or (1 << 5)
```
