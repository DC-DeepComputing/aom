---
name: libaom/remove-obsolete-config-guards-when-feature-standardized
description: When standardizing or enabling features across default builds, remove obsolete preprocessor configuration guards (#if CONFIG_...) in shared helper and query functions
rubric: correctness
severity: nit
---

**Rule.** When standardizing, enabling, or generalizing a feature, optimization,
or mode across default builds (such as filter tap configurations), remove
obsolete preprocessor configuration guards (`#if CONFIG_...` / `#else`) in
shared helper, query, and dispatch functions. Ensure shared helpers return true
capabilities or evaluate actual parameters across all builds rather than
clamping values or returning hardcoded fallbacks behind legacy feature flags.

**Why.** Leaving obsolete `#if CONFIG_...` guards in shared helper or query
functions causes baseline builds to take outdated fallback paths or misreport
supported capabilities, preventing standard builds from detecting, dispatching,
or validating the generalized feature.

## Examples

Bad:

```cpp
// When 2-tap filtering is generalized across standard builds:
#if CONFIG_SVT_AV1
  if (filter[2] || filter[5]) {
    return 4;
  }
  return 2;
#else
  return 4;
#endif
```

Good:

```cpp
// Remove obsolete CONFIG guard so standard builds evaluate 2-tap filters:
  if (filter[2] || filter[5]) {
    return 4;
  }
  return 2;
```
