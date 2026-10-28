---
name: libaom/av1/parameterize-motion-search-ranges-and-limits
description: When implementing motion estimation, search, or optimization routines, pass search ranges and window bounds as function parameters instead of hardcoding fixed constants
rubric: design
severity: should_fix
---

**Rule.** Pass algorithm search boundaries, radii, and iteration limits (such as
motion search ranges or window limits) as explicit function parameters rather
than hardcoding fixed numeric constants or macros inside search routines.
Validate or assert that dynamic search limits are positive (e.g., search_range >
0) before executing search loops.

**Why.** Hardcoding search limits inside search and estimation helpers locks
caller implementations to fixed iteration bounds, preventing callers from
adapting search scope, tuning speed versus quality tradeoffs, or reusing search
kernels across different resolutions or speed presets.

## Examples

Bad:

```cpp
// Bad: Hardcoding search range prevents caller configurability across presets.
void av1_motion_search_brute_force_erp(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       SphereMV *best_mv) {
  // Search range is hardcoded to 30 pixels
  for (int i = -30; i <= 30; ++i) {
    for (int j = -30; j <= 30; ++j) {
      evaluate_sphere_point(src, src_stride, ref, ref_stride, i, j, best_mv);
    }
  }
}
```

Good:

```cpp
// Good: Search range is parameterized and asserted positive.
void av1_motion_search_brute_force_erp(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       int search_range, SphereMV *best_mv) {
  assert(search_range > 0);
  for (int i = -search_range; i <= search_range; ++i) {
    for (int j = -search_range; j <= search_range; ++j) {
      evaluate_sphere_point(src, src_stride, ref, ref_stride, i, j, best_mv);
    }
  }
}
```
