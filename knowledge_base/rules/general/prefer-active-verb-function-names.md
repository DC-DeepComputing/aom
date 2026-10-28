---
name: libaom/prefer-active-verb-function-names
description: When naming action or state-transition helper functions, use concise active verb phrases rather than verbose patterns like set_<noun>_as_<status>
rubric: readability
severity: nit
---

**Rule.** When naming helper, action, or state-transition functions, use
concise, direct active verb phrases (e.g., `resolve_dependency()`) rather than
verbose descriptive patterns such as `set_<noun>_as_<status>` (e.g.,
`set_dependency_as_resolved()`).

**Why.** Direct active verb phrases describe the operation concisely and are
more idiomatic in C than wordy state descriptions, improving readability at
declaration and call sites.

## Examples

Bad:

```cpp
static AOM_INLINE void set_loop_filter_dependency_as_resolved(AV1_COMP *cpi) {
  cpi->mt_info.loop_filter_dependency_resolved = true;
}
```

Good:

```cpp
static AOM_INLINE void resolve_loop_filter_dependency(AV1_COMP *cpi) {
  cpi->mt_info.loop_filter_dependency_resolved = true;
}
```
