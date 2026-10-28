---
name: libaom/preserve-valid-input-pointers-on-error
description: When handling error paths in functions taking caller input and output pointers, do not nullify valid input pointers; only reset failed output handles.
rubric: correctness
severity: nit
---

**Rule.** Do not overwrite, clear, or nullify valid caller-owned input pointers
or resource handles on error paths when an operation (such as allocating or
transforming an output resource) fails. On failure, only reset dedicated output
pointers or free newly created temporary allocations, leaving caller-provided
input references intact.

**Why.** Overwriting or nullifying caller input pointers on error paths destroys
the caller's reference to its existing valid resources. This causes resource
leaks or null-pointer crashes if the caller inspects or attempts to clean up its
input pointers after a failed call.

## Examples

Bad:

```cpp
if (!img_shifted) {
  *img_shifted_ptr = NULL;
  *img_ptr = NULL;  // Destroys caller's valid input pointer
  return false;
}
```

Good:

```cpp
if (!img_shifted) {
  *img_shifted_ptr = NULL;
  return false;
}
```
