---
name: libaom/av1/use-av1-prefix-not-speculative-prefixes
description: When defining new functions, helpers, or experimental features in the av1/ codebase, use the established av1_ prefix instead of speculative prefixes like av2_
rubric: readability
severity: should_fix
---

**Rule.** When adding functions, internal helpers, or experimental tools within
the av1/ subsystem, prefix their identifiers with av1_ (or aom_ where
appropriate) rather than speculative or forward-looking prefixes such as av2_.
Keep all experimental features under the established av1_ naming convention
until an official, coordinated codebase migration occurs.

**Why.** Using speculative prefixes like av2_ in the av1/ codebase creates
naming fragmentation and breaks consistency with thousands of existing subsystem
APIs. Standardizing on av1_ ensures clean symbol grepping, consistent
namespacing, and avoids ad-hoc versioning before formal repository
restructuring.

## Examples

Bad:

```cpp
// Speculative prefix in av1/ subsystem
int32_t *av2_warp_rotation(MB_MODE_INFO *mi, int8_t rotation, int mi_x,
                           int mi_y) {
  // ...
}
```

Good:

```cpp
// Established av1_ prefix consistent with the subsystem
int32_t *av1_warp_rotation(MB_MODE_INFO *mi, int8_t rotation, int mi_x,
                           int mi_y) {
  // ...
}
```
