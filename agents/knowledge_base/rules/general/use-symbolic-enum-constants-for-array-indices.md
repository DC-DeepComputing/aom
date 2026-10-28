---
name: libaom/use-symbolic-enum-constants-for-array-indices
description: When indexing arrays or multidimensional tables sized by enum constants, use symbolic enum values rather than bare integer literals
rubric: readability
severity: nit
---

**Rule.** When indexing arrays or multidimensional tables dimensioned by enum
types (such as color planes, prediction types, transform sizes, or frame types),
use symbolic enum constants rather than bare integer literals (such as 0 or 1).
Symbolic indexing makes the plane or mode intent explicit and avoids subtle
index transposition bugs.

**Why.** Bare integer indices obscure which semantic dimension (e.g., U vs. V
plane, luma vs. chroma) is being accessed, reducing code readability and
increasing the risk of off-by-one or mismatched index bugs when enum ordering
changes.

## Examples

Bad:

```cpp
// Bare integer indices obscure whether index 0 refers to U or V plane
cfl->dc_pred_is_cached[0] = false;
cfl->dc_pred_is_cached[1] = false;
int cost_u = mode_costs->cfl_cost[joint_sign][0][alpha_u];
```

Good:

```cpp
// Symbolic enum constants make plane dimensions clear and maintainable
cfl->dc_pred_is_cached[CFL_PRED_U] = false;
cfl->dc_pred_is_cached[CFL_PRED_V] = false;
int cost_u = mode_costs->cfl_cost[joint_sign][CFL_PRED_U][alpha_u];
```
