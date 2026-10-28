---
name: libaom/av1/return-best-distortion-from-motion-search
description: When defining motion search or block matching functions, return the best distortion metric or matching cost instead of returning void
rubric: readability
severity: should_fix
---

**Rule.** Design motion search, spherical projection search, and block matching
functions to return the best matching distortion metric or cost (such as
best_sad, best_sse, or bestsme) rather than returning void. While writing output
motion vectors or parameters through pointer arguments, return the minimal cost
directly so callers can evaluate match quality or compare candidates without
recomputing distortion.

**Why.** Motion estimation routines evaluate and track minimal distortion (SAD
or SSE) internally during candidate searching. Returning this cost directly
gives callers immediate access to match quality for mode decisions and pruning
without incurring redundant metric recomputation or cluttering parameter lists
with extra distortion pointers.

## Examples

Bad:

```cpp
// Bad: Returning void discards the computed minimal distortion/SAD.
void av1_motion_search_brute_force_erp(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       int search_range, SphereMV *best_mv) {
  int best_sad = INT_MAX;
  // Search loops find best_mv and calculate best_sad...
}
```

Good:

```cpp
// Good: Returns the best SAD distortion metric for caller evaluation.
int av1_motion_search_brute_force_erp(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride,
                                      int search_range, SphereMV *best_mv) {
  int best_sad = INT_MAX;
  // Search loops find best_mv and calculate best_sad...
  return best_sad;
}
```
