---
name: libaom/separate-renaming-and-refactoring-from-bug-fixes
description: When preparing feature or bug-fix CLs, separate unrelated fixes and refactorings into dedicated commits, and describe any bundled secondary adjustments in the commit message
rubric: change_scope
severity: should_fix
---

**Rule.** Do not combine unrelated bug fixes, broad identifier renaming,
cosmetic cleanups, or structural refactorings with feature work or subtle
bug-fix changes in the same CL; isolate unrelated bug fixes and refactorings
into separate, dedicated commits. When a CL intentionally includes minor
secondary fixes or drive-by adjustments alongside the primary feature or
optimization, explicitly describe those secondary changes in the commit message
body.

**Why.** Bundling unrelated bug fixes, renamings, or refactorings into feature
CLs complicates code review, testing, regression tracking, and cherry-picking or
rollbacks. Isolating unrelated changes keeps reviews focused and git history
bisectable, while documenting bundled secondary adjustments ensures reviewers
and future maintainers understand all modified logic.

## Examples

Bad:

```cpp
// 1. Mixing unrelated bug fixes into a feature CL:
// In a feature CL implementing variance boost:
-  DELTAQ_MODE deltaq_strength;
+  int deltaq_strength;

// 2. Mixing broad identifier renames into a subtle bug fix:
-  InitialDimensions initial_dimensions;
+  InitialDimensions allocated_dimensions;

// 3. Omitting bundled secondary fixes from the commit message:
// Commit message:
// rtc: Speedup for dynamic screen content
// Detect if content has high motion from source_sad...
```

Good:

```cpp
// 1. Keep feature and bug-fix CLs focused; submit unrelated fixes in dedicated CLs:
// Keep feature CL focused and submit deltaq_strength type fix in a separate CL.

// 2. Defer identifier renamings to a separate follow-up CL:
   InitialDimensions initial_dimensions;

// 3. Explicitly document bundled secondary fixes in the commit message:
// Commit message:
// rtc: Speedup for dynamic screen content
// Detect if content has high motion from source_sad...
//
// This also contains a fix to bsize_select for the fixed partitioning.
```
