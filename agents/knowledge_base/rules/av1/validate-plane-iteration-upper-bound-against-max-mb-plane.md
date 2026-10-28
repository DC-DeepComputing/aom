---
name: libaom/av1/validate-plane-iteration-upper-bound-against-max-mb-plane
description: When validating exclusive upper bounds for plane iteration in half-open ranges, check against plane count MAX_MB_PLANE rather than last-plane index AOM_PLANE_V
rubric: correctness
severity: should_fix
---

**Rule.** When validating or bounding exclusive upper bounds for plane iteration
(such as `plane_end` in half-open ranges `[plane_start, plane_end)` or loops
`for (plane = plane_start; plane < plane_end; plane++)`), check against the
maximum plane count limit `MAX_MB_PLANE` (or `av1_num_planes(cm)`), not the
zero-based index of the last plane (`AOM_PLANE_V`).

**Why.** `AOM_PLANE_V` is the 0-based index of the third plane (value 2),
whereas full plane iteration requires an exclusive upper bound equal to the
plane count (value 3 / `MAX_MB_PLANE`). Asserting `plane_end <= AOM_PLANE_V`
creates an off-by-one error that either causes valid assertions to fail or
terminates iteration before the V plane is processed.

## Examples

Bad:

```cpp
// plane_end is an exclusive upper bound, but AOM_PLANE_V is the plane index (value 2).
assert(plane_end <= AOM_PLANE_V);
for (plane = plane_start; plane < plane_end; plane++) {
  process_plane(plane);
}
```

Good:

```cpp
// Exclusive upper bounds must be validated against the plane count (MAX_MB_PLANE == 3).
assert(plane_start >= AOM_PLANE_Y);
assert(plane_end <= MAX_MB_PLANE);
for (plane = plane_start; plane < plane_end; plane++) {
  process_plane(plane);
}
```
