---
name: libaom/av1/accurately-name-resolution-range-check-variables
description: When creating boolean condition variables for resolution range checks, name them to reflect the actual evaluated bounds (e.g., is_between_480p_and_720p) instead of single-resolution shorthand
rubric: readability
severity: should_fix
---

**Rule.** Name boolean condition and range check variables to accurately reflect
the actual bounds evaluated (such as is_between_480p_and_720p,
is_720p_or_larger, or is_360p_or_smaller) rather than naming after a single
target resolution (e.g., is_short_608p) or assuming a specific video orientation
when testing min/max dimensions.

**Why.** Naming a boolean variable after a single resolution or assuming a
specific orientation when it actually checks a range of dimensions obscures the
condition's true semantics, misleads readers about which video formats trigger
the logic, and hinders code maintenance.

## Examples

Bad:

```cpp
// Bad: named after a single target resolution despite evaluating a range
const bool is_short_608p = AOMMIN(cm->width, cm->height) > 480 &&
                           AOMMIN(cm->width, cm->height) < 720;
if (is_short_608p) {
  sf->part_sf.max_intra_bsize = BLOCK_32X32;
}
```

Good:

```cpp
// Good: accurately names the lower and upper bounds evaluated
const bool is_between_480p_and_720p = AOMMIN(cm->width, cm->height) > 480 &&
                                      AOMMIN(cm->width, cm->height) < 720;
if (is_between_480p_and_720p) {
  sf->part_sf.max_intra_bsize = BLOCK_32X32;
}
```
