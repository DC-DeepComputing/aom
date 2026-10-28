---
name: libaom/preserve-explanatory-comments-when-extracting-helpers
description: When extracting, moving, or refactoring functions and inline logic, preserve existing doc comments and explanatory rationale regarding invariants, parameter/return semantics, and side effects
rubric: readability
severity: nit
---

**Rule.** When extracting, moving, or refactoring existing functions, helpers,
or inline code blocks, preserve all existing doc comments and explanatory
comments. Ensure that comments explaining function contracts (such as return
value conventions, parameter semantics, and preconditions) as well as inline
comments detailing non-obvious rationale, hazards, side effects, or invariants
(e.g., why specific state, cache, or flags must be cleared) are moved into the
new function or helper definition rather than dropped.

**Why.** Refactoring and relocating code frequently results in dropped comments,
losing critical context about function contracts (such as error codes versus
boolean return values) and non-obvious implementation hazards (such as state or
cache invalidation). Retaining doc comments and rationale ensures safety
invariants and design intent remain clear to future readers across all call
sites without forcing them to deduce contracts from implementation details.

## Examples

Bad:

```cpp
// Variant 1: Function moved across files without retaining doc comment
static int valid_obu_type(int obu_type) {
  int valid_type = 0;
  switch (obu_type) {
    case OBU_SEQUENCE_HEADER:
    case OBU_TEMPORAL_DELIMITER:
    case OBU_FRAME_HEADER:
    case OBU_TILE_GROUP:
    case OBU_PADDING: valid_type = 1; break;
    default: break;
  }
  return valid_type;
}

// Variant 2: Inline logic extracted to helper without retaining rationale
static INLINE void clear_cfl_dc_pred_flags(CFL_CTX *cfl) {
  cfl->use_dc_pred_cache = 0;
  cfl->dc_pred_is_cached[CFL_PRED_U] = 0;
  cfl->dc_pred_is_cached[CFL_PRED_V] = 0;
}
```

Good:

```cpp
// Variant 1: Retain doc comment explaining return value convention
// Returns 1 when OBU type is valid, and 0 otherwise.
static int valid_obu_type(int obu_type) {
  int valid_type = 0;
  switch (obu_type) {
    case OBU_SEQUENCE_HEADER:
    case OBU_TEMPORAL_DELIMITER:
    case OBU_FRAME_HEADER:
    case OBU_TILE_GROUP:
    case OBU_PADDING: valid_type = 1; break;
    default: break;
  }
  return valid_type;
}

// Variant 2: Preserve explanatory comment inside extracted helper
static INLINE void clear_cfl_dc_pred_cache_flags(CFL_CTX *cfl) {
  // Clear the following flags to avoid unintentional usage of cached dc pred data.
  cfl->use_dc_pred_cache = 0;
  cfl->dc_pred_is_cached[CFL_PRED_U] = 0;
  cfl->dc_pred_is_cached[CFL_PRED_V] = 0;
}
```
