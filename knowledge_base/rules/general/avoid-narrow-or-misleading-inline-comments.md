---
name: libaom/avoid-narrow-or-misleading-inline-comments
description: When commenting shared setup or multi-mode logic, ensure inline comments reflect the full scope rather than describing only a single mode or feature
rubric: readability
severity: should_fix
---

**Rule.** When writing inline comments for shared setup, initialization, or
preprocessing logic that serves multiple operating modes or pipelines, ensure
comments accurately describe the full scope rather than attributing the code to
a single specific mode. Remove or update overly narrow, misleading, or redundant
inline comments that fail to reflect all active use cases.

**Why.** Inline comments that attribute shared setup logic to only a single
feature or mode mislead readers into assuming the code is inactive or
unnecessary in other configurations, obscuring multi-mode dependencies and
complicating maintenance.

## Examples

Bad:

```cpp
  // Delta-q modulation based on variance
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes, sb_size);
```

Good:

```cpp
  // Set up source planes for delta-q modulation analysis across active modes
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes, sb_size);
```
