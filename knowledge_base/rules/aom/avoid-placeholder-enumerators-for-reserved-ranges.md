---
name: libaom/aom/avoid-placeholder-enumerators-for-reserved-ranges
description: When defining enums with reserved or future value ranges in public headers (aom/*.h), do not declare placeholder enumerators; document reserved ranges in comments instead
rubric: readability
severity: should_fix
---

**Rule.** In public API headers (aom/*.h), do not define dummy placeholder
enumerators (such as *_RESERVED_* or *_FUTURE_*) for unassigned or reserved
value ranges. Document reserved integer ranges using Doxygen comments instead.

**Why.** Defining named enumerator constants for reserved ranges binds those
identifiers into the public API and ABI. When unassigned values are later
assigned concrete specification names, the placeholder constants cannot be
removed without breaking ABI checks, forcing the library to maintain redundant
aliases indefinitely.

## Examples

Bad:

```cpp
typedef enum aom_cicp_matrix_coefficients {
  AOM_CICP_MC_YCGCO_RE = 16, /**< YCgCo-Re */
  AOM_CICP_MC_YCGCO_RO = 17, /**< YCgCo-Ro */
  // Bad: binds placeholder symbol into public ABI
  AOM_CICP_MC_RESERVED_18 = 18 /**< For future use (values 18-255) */
} aom_cicp_matrix_coefficients_t;
```

Good:

```cpp
typedef enum aom_cicp_matrix_coefficients {
  AOM_CICP_MC_YCGCO_RE = 16, /**< YCgCo-Re */
  AOM_CICP_MC_YCGCO_RO = 17  /**< YCgCo-Ro */
  /**< For future use (values 18-255) */
} aom_cicp_matrix_coefficients_t;
```
