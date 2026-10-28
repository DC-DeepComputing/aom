---
name: libaom/assign-variables-close-to-use
description: When initializing or assigning local variables for multi-step routines or loops, place assignments immediately before their consuming block rather than grouping setup upfront
rubric: readability
severity: nit
---

**Rule.** Place variable assignments, pointer computations, and initializations
immediately before the specific loop or block that consumes them rather than
grouping all setup at the beginning of a multi-step routine. Keep variable setup
adjacent to its corresponding processing phase to make dataflow and lifetimes
explicit.

**Why.** Assigning or initializing variables far ahead of the loop or block that
consumes them separates them from their usage context, reducing readability and
misleading readers into thinking intervening code depends on or alters them.

## Examples

Bad:

```cpp
src_ptr1 = src - extend_left;
src_ptr2 = src_ptr1 + src_stride * (height - 1);
dst_ptr1 = src_ptr1 + src_stride * -extend_top;
dst_ptr2 = src_ptr2;

for (i = 0; i < extend_top; ++i) {
  memcpy(dst_ptr1, src_ptr1, linesize);
  dst_ptr1 += src_stride;
}

for (i = 0; i < extend_bottom; ++i) {
  dst_ptr2 += src_stride;
  memcpy(dst_ptr2, src_ptr2, linesize);
}
```

Good:

```cpp
src_ptr1 = src - extend_left;
dst_ptr1 = src_ptr1 + src_stride * -extend_top;

for (i = 0; i < extend_top; ++i) {
  memcpy(dst_ptr1, src_ptr1, linesize);
  dst_ptr1 += src_stride;
}

src_ptr2 = src_ptr1 + src_stride * (height - 1);
dst_ptr2 = src_ptr2;

for (i = 0; i < extend_bottom; ++i) {
  dst_ptr2 += src_stride;
  memcpy(dst_ptr2, src_ptr2, linesize);
}
```
