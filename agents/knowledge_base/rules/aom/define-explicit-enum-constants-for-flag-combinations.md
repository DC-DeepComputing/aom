---
name: libaom/aom/define-explicit-enum-constants-for-flag-combinations
description: When using enum types for bitwise flags in public C API parameters, define explicit enum constants for valid flag combinations so C++ callers avoid explicit casts
rubric: readability
severity: should_fix
---

**Rule.** In public C API headers, when an enum type is used as a function
parameter type for bitwise flags rather than an integer type, define explicit
enum constants for all valid bitwise flag combinations. This ensures C++ callers
can pass combined flags directly without requiring explicit type casts.

**Why.** In C++, bitwise operations between enum values produce integer types
that do not implicitly convert back to the enum parameter type. Providing named
enumerator constants for valid flag combinations enables clean, type-safe usage
in both C and C++ without forcing callers to write static_cast or C-style casts.

## Examples

Bad:

```cpp
typedef enum aom_metadata_insert_flags {
  AOM_MIF_KEY_FRAME = 1,
  AOM_MIF_LAYER_SPECIFIC = 0x10,
} aom_metadata_insert_flags_t;

// In C++, passing (AOM_MIF_KEY_FRAME | AOM_MIF_LAYER_SPECIFIC)
// causes a compilation error without an explicit cast.
aom_codec_err_t aom_img_add_metadata(aom_image_t *img, uint32_t type,
                                     const uint8_t *data, size_t sz,
                                     aom_metadata_insert_flags_t insert_flag);
```

Good:

```cpp
typedef enum aom_metadata_insert_flags {
  AOM_MIF_KEY_FRAME = 1,
  AOM_MIF_LAYER_SPECIFIC = 0x10,
  AOM_MIF_KEY_FRAME_LAYER_SPECIFIC = 0x11,
} aom_metadata_insert_flags_t;

// C++ callers can pass AOM_MIF_KEY_FRAME_LAYER_SPECIFIC directly without casting.
aom_codec_err_t aom_img_add_metadata(aom_image_t *img, uint32_t type,
                                     const uint8_t *data, size_t sz,
                                     aom_metadata_insert_flags_t insert_flag);
```
