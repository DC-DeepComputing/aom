---
name: libaom/remove-git-merge-conflict-markers
description: When resolving merge or rebase conflicts, remove all git conflict markers such as <<<<<<<, =======, and >>>>>>> before submitting changes
rubric: readability
severity: blocking
---

**Rule.** Do not leave git merge conflict markers (such as `<<<<<<< HEAD`,
`=======`, `|||||||`, or `>>>>>>>`) or unresolved merge artifacts in committed
files. Verify that all merge or rebase conflicts are cleanly resolved before
submitting changes.

**Why.** Unresolved conflict markers cause compilation and syntax failures in
source files and corrupt build scripts or documentation, indicating incomplete
merge resolution.

## Examples

Bad:

```cpp
struct CNN_LAYER_CONFIG {
<<<<<<< HEAD
  int deconvolve;
=======
  int deconv;
>>>>>>> branch
};
```

Good:

```cpp
struct CNN_LAYER_CONFIG {
  int deconvolve;
};
```
