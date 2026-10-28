---
name: libaom/distinguish-array-elements-from-indices-in-comments
description: When documenting array contents, values, or data structures in comments, refer to array elements or entries rather than array indices
rubric: readability
severity: nit
---

**Rule.** In comments and documentation, use precise terminology when describing
arrays: refer to 'array elements' or 'entries' when discussing the stored values
or contents, and reserve 'array indices' or 'subscripts' for their positions and
offsets. Do not describe array indices as holding or having data values.

**Why.** An array index designates the position or subscript within an array,
whereas an element or entry is the value or variable stored at that position.
Conflating indices with elements in technical comments confuses readers about
whether a condition or invariant applies to array keys/positions or the values
stored within them.

## Examples

Bad:

```cpp
// - Total 'n' of the variables / array indices above have value 'k'
// - Each array index holds a pointer to the active reference buffer
```

Good:

```cpp
// - Total 'n' of the variables / array elements above have value 'k'
// - Each array element holds a pointer to the active reference buffer
```
