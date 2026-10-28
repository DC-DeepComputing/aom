---
name: libaom/avoid-drive-by-refactoring-in-bug-fixes
description: When making bug fixes or targeted changes, avoid stray blank lines, whitespace modifications, or unrelated refactorings; keep diffs focused on their primary intent
rubric: change_scope
severity: should_fix
---

**Rule.** Do not mix unrelated code cleanups, reformatting, stray whitespace or
blank lines, or function signature modernizations into focused bug fixes or
feature changes. Keep each CL focused on a single logical change and submit
drive-by refactorings or cleanups in separate dedicated CLs.

**Why.** Extraneous whitespace modifications and unrelated cleanups create
review noise, obscure the primary change, clutter git blame history, and
complicate git bisect, cherry-picking, and backporting to release branches.

## Examples

Bad:

```cpp
/* 1. Stray blank lines in untouched code: */
  segmentation_data->delta_q_size = 3u;

  return true;

/* 2. Mixing unrelated signature refactoring into a bug-fix CL: */
-int aom_img_read(aom_image_t *img, FILE *file);
+bool aom_img_read(aom_image_t *img, FILE *file);
```

Good:

```cpp
/* 1. Keep diffs clean without extraneous whitespace modifications: */
  segmentation_data->delta_q_size = 3u;
  return true;

/* 2. Submit signature modernization in a separate follow-up CL: */
 int aom_img_read(aom_image_t *img, FILE *file);
```
