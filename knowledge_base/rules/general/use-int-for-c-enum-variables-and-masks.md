---
name: libaom/use-int-for-c-enum-variables-and-masks
description: When declaring variables that store, mask, or compare against C enum constants, use int rather than uint32_t or unsigned types
rubric: readability
severity: nit
---

**Rule.** In C source and header files, declare variables that hold, mask, or
compare against enumeration constants as int rather than fixed-width unsigned
types such as uint32_t or unsigned int. In C99, enumeration constants have type
int, so using plain int preserves standard enum semantics and avoids
signed/unsigned comparison mismatches.

**Why.** In standard C (C99), enum constants evaluate to signed int. Storing
enum values or masked enum bitfields in unsigned types like uint32_t introduces
unnecessary type conversions and can trigger -Wsign-compare compiler warnings
when comparing the variable against other enum constants.

## Examples

Bad:

```cpp
// Storing masked enum value in uint32_t causes signed/unsigned mismatch with enum constants:
const uint32_t metadata_insert_location =
    (current_metadata->insert_flag) & AOM_MIF_INSERT_LOCATION_MASK;
if (metadata_insert_location == AOM_MIF_KEY_FRAME) {
  // ...
}
```

Good:

```cpp
// Enum constants have type int in C99; plain int matches enum semantics:
const int metadata_insert_location =
    current_metadata->insert_flag & AOM_MIF_INSERT_LOCATION_MASK;
if (metadata_insert_location == AOM_MIF_KEY_FRAME) {
  // ...
}
```
