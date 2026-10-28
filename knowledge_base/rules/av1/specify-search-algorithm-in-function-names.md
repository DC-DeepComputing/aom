---
name: libaom/av1/specify-search-algorithm-in-function-names
description: When declaring or defining specialized search, estimation, or optimization functions, specify the concrete algorithm or strategy in the function name
rubric: readability
severity: nit
---

**Rule.** When defining specialized search, estimation, or optimization
functions (such as motion estimation, intra/inter mode search, or spherical
projection search), qualify the function name with the specific algorithm or
strategy employed (e.g., `_brute_force_`, `_diamond_`, `_fast_`, `_exhaustive_`,
or `_full_search_`) rather than using an overly generic name (like
`av1_motion_search_erp`). Reserve unqualified names for top-level search entry
points and facade dispatchers.

**Why.** Generic names obscure the underlying search algorithm, creating
ambiguity at call sites and risking naming collisions or refactoring churn when
multiple search heuristics, approximations, or dispatchers are introduced.

## Examples

Bad:

```cpp
// Ambiguous: generic function name obscures that it performs a brute-force search.
void av1_motion_search_erp(int block_x, int block_y, const MACROBLOCK *x,
                           MACROBLOCKD *xd, ...);
```

Good:

```cpp
// Explicit: specifies the brute-force search algorithm directly in the name.
void av1_motion_search_brute_force_erp(int block_x, int block_y,
                                       const MACROBLOCK *x, MACROBLOCKD *xd,
                                       ...);
```
