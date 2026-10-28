---
name: libaom/separate-unrelated-bug-fixes
description: When submitting a feature, optimization, or refactoring CL, do not bundle unrelated bug fixes or drive-by cleanups; separate them into standalone CLs.
rubric: change_scope
severity: should_fix
---

**Rule.** Do not bundle unrelated bug fixes, formatting adjustments, or drive-by
refactorings into a feature or optimization CL. Keep each CL focused on a single
logical change, and isolate independent bug fixes into separate, dedicated CLs
submitted beforehand.

**Why.** Bundling unrelated fixes into feature or optimization changes expands
review scope, obscures the core functionality being modified, and makes
individual bug fixes difficult to bisect, cherry-pick to release branches, or
revert if regressions occur.

## Examples

Bad:

```cpp
// In a CL adding SSSE3 general scaling optimization:
static void scale_plane_ssse3(const uint8_t *src, int src_stride, ...) {
  // ... optimization implementation ...
}

// Drive-by bug fix in unrelated fallback scaling logic bundled into the same CL:
int scale_plane_c(const uint8_t *src, int src_stride, ...) {
  if (dst_w == src_w * 2 && dst_h == src_h * 2) {
    uint8_t *const temp = (uint8_t *)malloc(8 * ((src_y_w + 7) & ~7));
    if (temp) {
      scaled = 1;  // Unrelated bug fix bundled into optimization CL
    }
  }
}
```

Good:

```cpp
// Dedicated CL 1 (standalone bug fix submitted first):
// Fix buffer allocation check in fallback scaling logic
int scale_plane_c(const uint8_t *src, int src_stride, ...) {
  if (dst_w == src_w * 2 && dst_h == src_h * 2) {
    uint8_t *const temp = (uint8_t *)malloc(8 * ((src_y_w + 7) & ~7));
    if (temp) {
      scaled = 1;
    }
  }
}

// Dedicated CL 2 (focused optimization CL landed after the fix):
// Add SSSE3 implementation for general scaling
static void scale_plane_ssse3(const uint8_t *src, int src_stride, ...) {
  // ... focused optimization without unrelated edits ...
}
```
