---
name: libaom/av1/name-mode-specific-predicates-precisely
description: When defining mode-specific predicates or eligibility helpers, name them precisely after the specific mode or subfeature evaluated rather than using overly generic names
rubric: readability
severity: should_fix
---

**Rule.** Name mode-specific predicates and feature-eligibility helpers
precisely after the specific mode, condition, or subfeature they evaluate (e.g.,
globalmv_rotation_allowed()) rather than using generic umbrella names (e.g.,
warp_rotation_allowed()). If a function restricts execution to a specific
prediction mode or motion mode, reflect that restriction directly in the
function name to prevent misleading callers about its scope of applicability.

**Why.** Overly broad or generic helper names lead callers and maintainers to
assume a check applies across all variants of a feature, obscuring underlying
mode preconditions and risking misuse at call sites.

## Examples

Bad:

```cpp
// Misleading: named after warped motion generally, but only applies to GLOBALMV.
static int warp_rotation_allowed(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *mbmi = xd->mi[0];
  if (mbmi->motion_mode != SIMPLE_TRANSLATION) return 0;
  if (mbmi->mode != GLOBALMV) return 0;
  return is_warp_rotation_allowed(xd);
}
```

Good:

```cpp
// Clear: explicitly named after the specific GLOBALMV mode it evaluates.
static int globalmv_rotation_allowed(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *mbmi = xd->mi[0];
  if (mbmi->motion_mode != SIMPLE_TRANSLATION) return 0;
  if (mbmi->mode != GLOBALMV) return 0;
  return is_warp_rotation_allowed(xd);
}
```
