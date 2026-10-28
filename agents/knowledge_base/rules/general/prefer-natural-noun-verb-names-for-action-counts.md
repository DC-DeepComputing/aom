---
name: libaom/prefer-natural-noun-verb-names-for-action-counts
description: When naming configuration or count fields and variables, prefer natural noun-verb phrases like channels_to_copy over awkward compound modifiers, documenting subset constraints in comments.
rubric: readability
severity: should_fix
---

**Rule.** Name configuration, count, and mask fields or variables using natural,
idiomatic noun-verb phrases (e.g., channels_to_copy, branches_to_combine,
bits_to_read) rather than awkward compound modifiers (e.g.,
initial_channels_copy). Document specific operational constraints, subset
boundaries, or indexing semantics (such as operating only on an initial
contiguous subset) in accompanying comments rather than encoding them into
convoluted identifier names.

**Why.** Natural English phrasing (<items>_to_<action>) makes struct fields and
variable names immediately recognizable and consistent with neighboring
identifiers across the codebase. Encoding detailed execution semantics or
boundary constraints into the identifier name produces awkward, confusing names,
whereas concise names paired with explanatory comments keep code clean and
readable.

## Examples

Bad:

```cpp
struct LayerConfig {
  // Awkward compound modifier encoding subset logic into the name
  int initial_channels_copy;  // Copies channels up to this index
  int subset_branches_merge;
};
```

Good:

```cpp
struct LayerConfig {
  // Natural noun-verb phrasing; subset semantics documented in comments
  int channels_to_copy;  // Number of initial channels (from index 0) to copy.
  int branches_to_combine;  // Bitmask of branches to combine with output.
};
```
