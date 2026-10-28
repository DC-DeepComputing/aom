---
name: libaom/avoid-bundling-unrelated-changes
description: When submitting changes, keep each CL focused on its stated intent; avoid bundling unrelated edits, or explicitly document secondary cleanups in the commit message
rubric: readability
severity: should_fix
---

**Rule.** Keep each change list (CL) strictly focused on its stated objective
and commit description. Do not bundle unrelated drive-by edits, incidental
refactoring, or work-in-progress code from other features; split separate
logical changes into independent CLs or explicitly document bundled secondary
cleanups in the commit description.

**Why.** Mixing unrelated edits or undocumented cleanups into a focused CL
obscures commit history, complicates code review, makes bisecting regressions
difficult, and leaves future maintainers without clear context for why changes
were made.

## Examples

Bad:

```cpp
// 1. Unrelated WIP feature field bundled into a bug-fix CL:
struct LayerConfig {
  int is_deconvolve;  // Unrelated feature field from another patch
  int in_channels;
};

// 2. Commit message omitting bundled secondary cleanups:
// Title: Add run-time flag for SDP
// Description: Currently both compile and run-time flags exist...
// (Diff also removes redundant default_angle_delta_cdf logic, but
//  the commit message does not mention or explain this cleanup.)
```

Good:

```cpp
// 1. Keep changes strictly focused solely on the bug fix:
struct LayerConfig {
  int in_channels;
};

// 2. Explicitly describe secondary cleanups, or split them into another CL:
// Title: Add run-time flag for SDP
// Description: Currently both compile and run-time flags exist...
//              In addition, redundant code regarding
//              default_angle_delta_cdf has been removed.
```
