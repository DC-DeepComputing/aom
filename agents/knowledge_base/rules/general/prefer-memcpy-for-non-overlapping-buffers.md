---
name: libaom/prefer-memcpy-for-non-overlapping-buffers
description: When copying data between distinct non-overlapping memory regions, use memcpy instead of memmove to avoid unnecessary overlap checks and overhead
rubric: safety
severity: should_fix
---

**Rule.** When copying buffer data between guaranteed non-overlapping memory
regions or distinct buffers, use `memcpy` rather than `memmove`. Reserve
`memmove` strictly for operations where the source and destination buffers may
overlap.

**Why.** `memmove` adds runtime overhead to check for and handle potential
buffer overlaps. When memory regions are known to be distinct, `memcpy` is
faster, enables compiler vectorization and inlining optimizations, and
explicitly signals the non-overlapping invariant.

## Examples

Bad:

```cpp
// Distinct buffers or non-overlapping rows copied with memmove
for (int r = 0; r < height; ++r) {
  memmove(dst + r * dst_stride, src + r * src_stride, width * sizeof(*src));
}
```

Good:

```cpp
// Use memcpy when source and destination do not overlap
for (int r = 0; r < height; ++r) {
  memcpy(dst + r * dst_stride, src + r * src_stride, width * sizeof(*src));
}
```
