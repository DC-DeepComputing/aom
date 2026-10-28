---
name: libaom/av1/name-syntax-presence-flags-explicitly
description: When passing syntax structure or header presence flags into parser functions, name them explicitly with the specific construct rather than generic names like has_extension
rubric: readability
severity: nit
---

**Rule.** Use explicit, disambiguated names for boolean flags indicating the
presence of specific syntax structures or headers (such as
has_obu_extension_header rather than has_extension), especially when passed into
payload parsers or helper functions that handle structures with their own
distinct extension fields.

**Why.** Generic names like has_extension or has_header are ambiguous in
payload-parsing functions (such as ITU-T T.35 metadata parsing) where
payload-level extension fields also exist. Specifying the exact syntax construct
clarifies which container or header level is being referenced.

## Examples

Bad:

```cpp
static void read_metadata_itut_t35(AV1Decoder *const pbi, const uint8_t *data,
                                   size_t sz, bool has_extension);
```

Good:

```cpp
static void read_metadata_itut_t35(AV1Decoder *const pbi, const uint8_t *data,
                                   size_t sz, bool has_obu_extension_header);
```
