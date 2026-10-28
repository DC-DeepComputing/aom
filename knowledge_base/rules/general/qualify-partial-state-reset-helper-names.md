---
name: libaom/qualify-partial-state-reset-helper-names
description: When naming helpers that clear or reset a specific subset of state or flags, include descriptive qualifiers (e.g., cache) instead of overly generic names
rubric: readability
severity: nit
---

**Rule.** When naming helper functions that reset, clear, or invalidate only a
specific subset of state, flags, or cache entries in a context or struct,
include explicit qualifiers (such as cache_flags or stats) in the function name.
Do not use overly broad or generic names (such as clear_..._flags or reset_...)
that misleadingly imply all state or configuration flags are being reset.

**Why.** Generic reset or clear helper names mislead readers and callers into
assuming the entire struct or subsystem state is reinitialized rather than just
a specific cache or tracking subset. Accurate qualifiers clarify side effects
and prevent callers from omitting other necessary state cleanup.

## Examples

Bad:

```cpp
// Ambiguous: implies all dc prediction flags or state are cleared
static INLINE void clear_cfl_dc_pred_flags(CFL_CTX *cfl) {
  cfl->use_dc_pred_cache = 0;
  cfl->dc_pred_is_cached[CFL_PRED_U] = 0;
  cfl->dc_pred_is_cached[CFL_PRED_V] = 0;
}
```

Good:

```cpp
// Explicit: qualifier clarifies only the cache validity flags are reset
static INLINE void clear_cfl_dc_pred_cache_flags(CFL_CTX *cfl) {
  cfl->use_dc_pred_cache = 0;
  cfl->dc_pred_is_cached[CFL_PRED_U] = 0;
  cfl->dc_pred_is_cached[CFL_PRED_V] = 0;
}
```
