---
name: libaom/av1/use-plane-index-not-plane-type-for-individual-planes
description: When declaring parameters or variables that distinguish individual color planes (Y, U, V), use int plane indices instead of the PLANE_TYPE enum
rubric: correctness
severity: should_fix
---

**Rule.** Do not use `PLANE_TYPE` for function parameters, return types, or
variables that need to distinguish individual color planes (such as Y, U, and
V). Use `int` (with plane index constants `AOM_PLANE_Y`, `AOM_PLANE_U`,
`AOM_PLANE_V`) for discrete plane indices, and reserve `PLANE_TYPE`
(`PLANE_TYPE_Y`, `PLANE_TYPE_UV`) strictly for luma versus chroma classification
(converting with `get_plane_type(plane)` when needed).

**Why.** In libaom, `PLANE_TYPE` only differentiates luma (`PLANE_TYPE_Y`) from
chroma (`PLANE_TYPE_UV`), grouping U and V together into `PLANE_TYPE_UV`. Using
`PLANE_TYPE` for operations that distinguish individual planes (such as CfL
prediction or per-plane buffer indexing) causes type confusion, invalid
arithmetic, and runtime logic errors.

## Examples

Bad:

```cpp
// Incorrect: PLANE_TYPE groups U and V together as PLANE_TYPE_UV
static INLINE CFL_PRED_TYPE get_cfl_pred_type(PLANE_TYPE plane) {
  assert(plane > 0);
  return (CFL_PRED_TYPE)(plane - 1);
}
```

Good:

```cpp
// Correct: uses int plane index (AOM_PLANE_Y=0, AOM_PLANE_U=1, AOM_PLANE_V=2)
static INLINE CFL_PRED_TYPE get_cfl_pred_type(int plane) {
  assert(plane > 0);
  return (CFL_PRED_TYPE)(plane - 1);
}
```
