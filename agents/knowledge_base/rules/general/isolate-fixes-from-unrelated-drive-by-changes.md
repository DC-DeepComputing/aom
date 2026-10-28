---
name: libaom/isolate-fixes-from-unrelated-drive-by-changes
description: When authoring or reviewing CLs, isolate bug fixes and tests from new feature additions, optimizations, code movement, and drive-by cleanups
rubric: change_scope
severity: should_fix
---

**Rule.** Keep bug fixes (and their accompanying test coverage), new feature
implementations or optimizations, and mechanical refactorings or code
reorganizations in separate, dedicated CLs. Do not bundle bug fixes into CLs
introducing new functions or optimizations, and do not mix functional behavior
changes with mechanical file movement, reordering, or unrelated drive-by
cleanups.

**Why.** Mixing independent concerns obscures behavioral changes and complicates
review. Furthermore, bundling bug fixes with new features or optimizations makes
rollbacks hazardous (reverting a faulty feature inadvertently undoes the bug
fix), while mixing logic edits with code movement impedes bisection and
cherry-picking.

## Examples

Bad:

```cpp
// Bad: Bundling independent changes across bug fixes, new features, or refactorings:
// Variant 1: Bundling a bug fix with a new feature in one CL:
// 1. Fix bug in highbd_quantize_b_neon() and update test randomization.
// 2. Add new aom_highbd_quantize_b_adaptive_neon() functions.

// Variant 2: Mixing functional logic with source file reorganizations in CMake:
- Invert conditional logic for source selection
- Reorder and reorganize all source list blocks across CMake files

// Variant 3: Drive-by error-handling cleanup in a performance fix CL:
struct aom_internal_error_info error;
AOM_CHECK_MEM_ERROR(&error, src_rgb, aom_malloc(buffer_size));
```

Good:

```cpp
// Good: Split independent concerns into focused, dedicated CLs:
// Variant 1: Dedicated CLs for bug fix vs new feature:
// CL 1: Fix bug in highbd_quantize_b_neon() and update test coverage.
// CL 2: Introduce new aom_highbd_quantize_b_adaptive_neon() functions.

// Variant 2: Dedicated CLs for functional logic vs mechanical reorganization:
// CL 1: Invert conditional logic for source selection.
// CL 2: Reorganize and consolidate CMake source lists.

// Variant 3: Submit unrelated error-handling or drive-by cleanups in a separate CL.
```
