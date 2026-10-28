---
name: libaom/traverse-buffer-rows-in-forward-memory-order
description: When iterating, copying, or extending 2D buffer rows, traverse lines in forward memory order with increasing addresses rather than decrementing row pointers
rubric: safety
severity: should_fix
---

**Rule.** Traverse and write 2D buffer lines sequentially in forward memory
order (increasing addresses) rather than backward or reverse order (decrementing
pointers by stride). When processing rows above a reference line (such as
extending top borders), precalculate the start offset at the lowest memory
address and increment pointers forward by stride on each line.

**Why.** Modern CPUs and hardware stream prefetchers are optimized for
sequential, forward memory access. Decrementing row pointers across 2D strides
disrupts prefetching and degrades data cache throughput, even if backward
traversal appears to simplify pointer initialization.

## Examples

Bad:

```cpp
dst_ptr1 = src_ptr1;
for (i = 0; i < extend_top; ++i) {
  dst_ptr1 -= src_stride;
  memcpy(dst_ptr1, src_ptr1, linesize);
}
```

Good:

```cpp
dst_ptr1 = src_ptr1 + src_stride * -extend_top;
for (i = 0; i < extend_top; ++i) {
  memcpy(dst_ptr1, src_ptr1, linesize);
  dst_ptr1 += src_stride;
}
```
