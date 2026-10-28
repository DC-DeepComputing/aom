---
name: libaom/av1/explain-speed-feature-preset-applicability
description: When documenting speed features restricted to specific speed presets or modes, explain the underlying architectural reason rather than just stating inapplicability
rubric: readability
severity: nit
---

**Rule.** When adding or updating comments for speed features, presets, or
encoder heuristics that are disabled or inapplicable at certain speed presets or
encoding modes (such as speed >= 7 or allintra), explain the underlying
architectural reason in the comment (such as switching from recursive search to
variance-based partitioning, or bypassing a search stage entirely). Do not
merely state that the feature is not applicable without explaining why.

**Why.** Simply stating that a speed feature is not applicable to certain speed
levels leaves maintainers unclear on whether the omission is intentional or an
oversight. Documenting the architectural reason (such as an alternative decision
algorithm or a bypassed evaluation stage) clarifies encoder behavior and
prevents unintended re-enabling or misconfigurations.

## Examples

Bad:

```cpp
// This speed feature is not applicable to speed >= 7.
bool prune_rect_part_using_none_pred_mode;
```

Good:

```cpp
// For speed >= 7, variance-based logic is used to determine the partition
// structure instead of recursive partition search. Therefore, this speed
// feature is not applicable in such cases.
bool prune_rect_part_using_none_pred_mode;
```
