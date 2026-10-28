---
name: libaom/match-enum-domain-in-mapping-tables
description: In argument parsing, lookup, or conversion tables (such as arg_enum_list), verify enum constants match the intended enum domain prefix rather than sibling enum types
rubric: correctness
severity: should_fix
---

**Rule.** In command-line argument mapping tables (such as `struct
arg_enum_list` arrays), codec configuration tables, and conversion mappings,
verify that all assigned enum constants belong to the intended enum domain and
identifier prefix (e.g., `AOM_CICP_TC_*` for transfer characteristics vs.
`AOM_CICP_CP_*` for color primaries or `AOM_CICP_MC_*` for matrix coefficients).
Do not mix or substitute constants from sibling enum families even if their
underlying integer values coincide.

**Why.** In C, unscoped enum types are weakly typed integers, so compilers do
not warn when a constant from a different enum domain is used in a mapping
table. Using mismatched enum constants harms code readability, obscures
configuration semantics, and risks silent logic errors if integer values diverge
in future updates or specifications.

## Examples

Bad:

```cpp
static const struct arg_enum_list transfer_characteristics_enum[] = {
  { "unspecified", AOM_CICP_CP_UNSPECIFIED },
  { "bt709", AOM_CICP_TC_BT_709 },
  { "srgb", AOM_CICP_TC_SRGB },
  { NULL, 0 }
};
```

Good:

```cpp
static const struct arg_enum_list transfer_characteristics_enum[] = {
  { "unspecified", AOM_CICP_TC_UNSPECIFIED },
  { "bt709", AOM_CICP_TC_BT_709 },
  { "srgb", AOM_CICP_TC_SRGB },
  { NULL, 0 }
};
```
