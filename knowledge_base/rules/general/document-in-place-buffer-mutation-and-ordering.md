---
name: libaom/document-in-place-buffer-mutation-and-ordering
description: When defining functions that combine, concatenate, or mutate buffers in place, document which parameter receives the output and the ordering of combined elements
rubric: readability
severity: nit
---

**Rule.** When defining or modifying functions that combine, concatenate, or
mutate buffers and data structures in place, precede the function with a comment
explicitly documenting the in-place contract. State which parameter receives the
final combined output and the ordering of combined or concatenated inputs.

**Why.** In-place buffer combination and mutation helpers can be ambiguous from
parameter names alone regarding whether the destination is overwritten or
appended to and which input's elements precede the other. Explicit documentation
prevents caller bugs and incorrect assumptions about output layout and ordering.

## Examples

Bad:

```cpp
static bool concat_tensor(const TENSOR *src, TENSOR *dst) {
  // Ambiguous whether dst channels precede src channels or vice versa.
  ...
}
```

Good:

```cpp
// The concatenated tensor is stored in dst, with original dst
// channels placed first followed by the channels from src.
static bool concat_tensor(const TENSOR *src, TENSOR *dst) {
  ...
}
```
