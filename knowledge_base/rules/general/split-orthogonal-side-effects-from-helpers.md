---
name: libaom/split-orthogonal-side-effects-from-helpers
description: When defining or modifying single-purpose helper functions, avoid bundling unrelated context or cache side effects into them; split orthogonal operations into separate helpers
rubric: design
severity: nit
---

**Rule.** Do not bundle unrelated side effects (such as clearing context-level
cache flags or resetting auxiliary state) into helper functions named for a
specific, narrower task. Keep helper functions single-purpose and extract
orthogonal state modifications or cache resets into separate, clearly named
helpers.

**Why.** Bundling orthogonal side effects into a narrowly named helper violates
single responsibility, conceals mutations from readers at call sites, and
prevents callers from reusing the helper when only the primary operation is
needed.

## Examples

Bad:

```cpp
static AOM_INLINE void set_invalid_cfl_parameters(
    CFL_CTX *cfl, uint8_t *best_cfl_alpha_idx, int8_t *best_cfl_alpha_signs) {
  *best_cfl_alpha_idx = 0;
  *best_cfl_alpha_signs = 0;

  // Unrelated cache flag clearing bundled into parameter reset helper:
  cfl->use_dc_pred_cache = 0;
  cfl->dc_pred_is_cached[0] = 0;
  cfl->dc_pred_is_cached[1] = 0;
}
```

Good:

```cpp
static AOM_INLINE void set_invalid_cfl_parameters(
    uint8_t *best_cfl_alpha_idx, int8_t *best_cfl_alpha_signs) {
  *best_cfl_alpha_idx = 0;
  *best_cfl_alpha_signs = 0;
}

static AOM_INLINE void clear_cfl_dc_pred_flags(CFL_CTX *cfl) {
  cfl->use_dc_pred_cache = 0;
  cfl->dc_pred_is_cached[CFL_PRED_U] = 0;
  cfl->dc_pred_is_cached[CFL_PRED_V] = 0;
}
```
