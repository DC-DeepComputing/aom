---
name: libaom/use-offset-or-idx-not-ptr-for-integer-offsets
description: When naming variables or struct members representing integer offsets or array indices, use suffixes like _offset or _idx instead of pointer suffixes like _ptr
rubric: readability
severity: nit
---

**Rule.** Do not use pointer suffixes (such as `_ptr`) when naming variables,
struct members, or function parameters that hold scalar integer offsets or array
indices. Use descriptive suffixes such as `_offset`, `_idx`, or `_index`
instead, reserving `_ptr` exclusively for variables storing memory addresses
(`T*`).

**Why.** Suffixing scalar integer offsets or indices with `_ptr` misleads
readers into expecting a memory address or pointer arithmetic (`T*`), causing
confusion when the variable is actually used for array indexing or stride
arithmetic.

## Examples

Bad:

```cpp
// tr_ptr is a scalar integer offset, not a memory pointer:
const int tr_ptr = tr_mask_row * xd->is_mi_coded_stride + tr_mask_col;
has_tr = xd->is_mi_coded[tr_ptr];
```

Good:

```cpp
// Use _offset or _idx for integer offsets and array indices:
const int tr_offset = tr_mask_row * xd->is_mi_coded_stride + tr_mask_col;
has_tr = xd->is_mi_coded[tr_offset];
```
