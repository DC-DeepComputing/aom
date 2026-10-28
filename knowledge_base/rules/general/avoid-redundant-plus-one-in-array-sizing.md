---
name: libaom/avoid-redundant-plus-one-in-array-sizing
description: When declaring fixed-size arrays indexed by capacity or count constants, size them directly without + 1 unless 1-based indexing or sentinel entries are required
rubric: readability
severity: should_fix
---

**Rule.** Do not add a redundant '+ 1' when sizing fixed arrays with capacity or
count constants (such as MAX_NUM_* or *_MAX) when elements are 0-indexed across
0 to MAX - 1. Size the array directly with [MAX_CONSTANT] unless 1-based
indexing, inclusive upper bounds (0..MAX), or sentinel elements are explicitly
used; if redundant sizing is inherited during refactoring and uncertain to
change immediately, document it with an attributed TODO comment.

**Why.** Adding an unnecessary +1 to array dimensions wastes memory and misleads
readers into assuming the upper index is valid or that the array uses 1-based
indexing. Explicitly sizing arrays to the exact count or documenting legacy
offsets prevents confusion and potential off-by-one errors.

## Examples

Bad:

```cpp
// Bad: Array is 0-indexed (0 to MAX_NUM_OPERATING_POINTS - 1), but + 1 is redundantly added.
uint32_t buffer_removal_times[MAX_NUM_OPERATING_POINTS + 1];
```

Good:

```cpp
// Good: Array is sized directly to the exact count of elements.
uint32_t buffer_removal_times[MAX_NUM_OPERATING_POINTS];

// Good: If inherited during refactoring and risky to modify immediately, document with a TODO.
// TODO(username): We probably don't need the +1 here.
uint32_t legacy_buffer_removal_times[MAX_NUM_OPERATING_POINTS + 1];
```
