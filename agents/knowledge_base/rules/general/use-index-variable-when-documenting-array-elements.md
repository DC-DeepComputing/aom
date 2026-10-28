---
name: libaom/use-index-variable-when-documenting-array-elements
description: When documenting array element semantics or valid values in comments, refer to elements using an index variable like array[i] rather than array[N]
rubric: readability
severity: nit
---

**Rule.** When documenting the semantics, valid value ranges, or behavior of
elements in an array or fixed-size buffer, refer to the element using an index
variable (such as `array[i]`) or empty brackets (`array[]`) rather than the
array's capacity or dimension (such as `array[N]`).

**Why.** Using the total size or bound (such as `array[N]`) to describe
element-level semantics is ambiguous and misleading, as `N` is an out-of-bounds
index for a 0-indexed array of size `N`. Using an index variable like `array[i]`
clearly indicates that the description applies to each individual element.

## Examples

Bad:

```cpp
// reference[7]: boolean flag indicating which of the 7 references are used.
// refresh[8]: boolean flag indicating if a buffer slot is updated.
int reference[7];
int refresh[8];
```

Good:

```cpp
// reference[i]: boolean flag indicating which of the 7 references are used.
// refresh[i]: boolean flag indicating if a buffer slot is updated.
int reference[7];
int refresh[8];
```
