---
name: libaom/omit-renaming-macros-on-static-template-functions
description: When defining static internal helper functions in C template include files (.inc / _template.h), do not wrap function names in symbol-renaming macros
rubric: readability
severity: nit
---

**Rule.** Do not wrap static internal helper functions in symbol-renaming macros
(such as RENAME(...)) within C template include files (.inc or _template.h)
instantiated across translation units. Reserve symbol-renaming macros strictly
for non-static functions with external linkage or functions requiring distinct
external caller names.

**Why.** Functions declared with static have internal linkage scoped to their
enclosing translation unit, preventing symbol collisions across separate .c
compilation units even when sharing identical names. Wrapping static helpers in
renaming macros introduces unnecessary macro indirection and reduces code
readability without providing linker safety.

## Examples

Bad:

```cpp
// In av1/common/reconinter_template.inc:
static AOM_INLINE void RENAME(build_inter_predictors_sub8x8)(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int plane, const MB_MODE_INFO *mi,
    int mi_x, int mi_y) {
  // ...
}
```

Good:

```cpp
// In av1/common/reconinter_template.inc:
static AOM_INLINE void build_inter_predictors_sub8x8(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int plane, const MB_MODE_INFO *mi,
    int mi_x, int mi_y) {
  // ...
}
```
