---
name: libaom/av1/name-projection-functions-source-to-target-space
description: When declaring geometric coordinate transformation or projection functions, name them after source and destination spaces with projection method suffixes
rubric: readability
severity: should_fix
---

**Rule.** When declaring or defining geometric coordinate transformation and
projection functions, name them explicitly after both the source and target
coordinate spaces (e.g., sphere_to_plane and plane_to_sphere) rather than using
ambiguous abbreviations or incomplete names (such as equ_to_plane), and qualify
the specific projection method as a suffix (e.g., _erp for equirectangular
projection).

**Why.** Projections (such as equirectangular projection) represent coordinate
mappings between spaces rather than standalone entities. Explicitly identifying
both source and target geometries alongside the projection type clarifies
coordinate frames and unambiguously distinguishes forward transformations from
reverse transformations.

## Examples

Bad:

```cpp
// Ambiguous name omitting source space and projection qualifier
void equ_to_plane(double phi, double theta, double *x, double *y, int width, int height);
```

Good:

```cpp
// Explicit source-to-target naming with projection method suffix
void av1_sphere_to_plane_erp(double phi, double theta, int width, int height, double *x, double *y);
```
