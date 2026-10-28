---
name: libaom/revert-files-without-substantive-changes
description: When submitting changes, revert files containing only incidental whitespace, formatting churn, or unrelated edits to keep review diffs minimal and focused
rubric: change_scope
severity: should_fix
---

**Rule.** Do not include files that contain only incidental whitespace
modifications, formatting churn, or unrelated drive-by edits. Revert untouched
or non-substantive file modifications so that change diffs remain minimal,
clean, and strictly focused on the intended feature or bug fix.

**Why.** Touching files without functional changes enlarges the patch set,
creates unnecessary reviewer burden, pollutes git blame history, and increases
the likelihood of merge conflicts.

## Examples

Bad:

```cpp
// In a CL implementing a new DSP feature, leaving an unrelated file with only incidental whitespace edits in the commit:
diff --git a/aom_dsp/x86/variance_avx2.c b/aom_dsp/x86/variance_avx2.c
--- a/aom_dsp/x86/variance_avx2.c
+++ b/aom_dsp/x86/variance_avx2.c
@@ -120,2 +120,1 @@
-
 AOM_VAR_NO_LOOP_AVX2(32, 16, 9, 512)
```

Good:

```cpp
// Revert unrelated files containing only whitespace or drive-by changes before uploading the CL:
git checkout origin/main -- aom_dsp/x86/variance_avx2.c
```
