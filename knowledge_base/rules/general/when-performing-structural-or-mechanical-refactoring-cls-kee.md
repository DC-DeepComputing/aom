---
name: libaom/when-performing-structural-or-mechanical-refactoring-cls-kee
description: When performing structural or mechanical refactoring CLs, keep changes focused by preserving existing comments as-is and deferring new documentation to follow-up CLs
rubric: change_scope
severity: nit
---

**Rule.** Keep structural and mechanical refactoring CLs (such as moving struct
fields, extracting helper types, or reorganizing files) strictly focused on
structural reorganization by preserving existing comments as-is. Defer new field
documentation, comment rewrites, and explanatory documentation expansions to a
dedicated follow-up CL.

**Why.** Introducing new or expanded comments during large structural
refactorings increases review scope and diff noise, obscuring pure mechanical
changes and making verification of behavioral equivalence harder. Splitting
comment improvements into a separate CL keeps both the refactoring and
documentation reviews clean and focused.

## Examples

Bad:

```cpp
// In a mechanical refactoring CL extracting CommonQuantParams from AV1_COMMON:
struct CommonQuantParams {
  // Base delta Q index applied to DC coefficients for luminance blocks in keyframes.
  int y_dc_delta_q;
  // Base delta Q index applied to AC coefficients for luminance blocks.
  int y_ac_delta_q;
  ...
};
```

Good:

```cpp
// In a mechanical refactoring CL extracting CommonQuantParams from AV1_COMMON:
// Preserve existing comments as-is during the structural move:
struct CommonQuantParams {
  int y_dc_delta_q;
  int y_ac_delta_q;
  ...
};
// Submit new field documentation and expanded comments in a dedicated follow-up CL.
```
