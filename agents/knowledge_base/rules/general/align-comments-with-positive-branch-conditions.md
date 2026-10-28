---
name: libaom/align-comments-with-positive-branch-conditions
description: When documenting conditional branches or if-else statements, phrase inline comments in terms of the positive condition that executes rather than stating inverse skip logic
rubric: readability
severity: nit
---

**Rule.** When writing inline comments to explain conditional branches or guard
statements, describe the action taken when the condition evaluates to true
rather than stating the inverse or what is skipped when the condition is false.
Phrase the comment directly in terms of when the branch executes (e.g., 'only
update X if Y is not set' rather than 'if Y is set don't update X').

**Why.** Comments that state the inverse of what a condition evaluates force
readers to perform mental boolean inversion to connect the documentation with
the code logic. Phrasing comments to match the direct true-branch execution path
keeps documentation aligned with control flow and reduces cognitive load during
code review and maintenance.

## Examples

Bad:

```cpp
// If cyclic refresh is set in active region don't unset it.
else if (seg_map[i] != CR_SEGMENT_ID_BOOST1 &&
         seg_map[i] != CR_SEGMENT_ID_BOOST2) {
  seg_map[i] = active_map[i];
}
```

Good:

```cpp
// In active region: only unset segmentation map if cyclic refresh is
// not set.
else if (seg_map[i] != CR_SEGMENT_ID_BOOST1 &&
         seg_map[i] != CR_SEGMENT_ID_BOOST2) {
  seg_map[i] = active_map[i];
}
```
