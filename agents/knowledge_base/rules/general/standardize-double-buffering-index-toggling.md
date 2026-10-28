---
name: libaom/standardize-double-buffering-index-toggling
description: When implementing double-buffering or ping-pong buffer loops, standardize index toggling using !idx in loop expressions rather than mixing alternating arithmetic styles
rubric: readability
severity: nit
---

**Rule.** When implementing double-buffering or ping-pong buffer alternating
loops, standardize index toggling idioms across related routines by updating the
source index with `src_idx = !src_idx` (in the loop step expression or body) and
deriving `dst_idx = !src_idx`. Avoid mixing inconsistent alternating update
expressions, such as `1 - idx` or separately tracking redundant state variables.

**Why.** Standardizing index toggling across related double-buffering and
multi-scale processing routines reduces cognitive load, eliminates redundant
state tracking, and prevents off-by-one or synchronization bugs between source
and destination buffer indices.

## Examples

Bad:

```cpp
int src_idx = 1;
int dst_idx = 0;
for (int sub_width = 4; sub_width <= block_size; sub_width *= 2) {
  src_idx = 1 - src_idx;
  dst_idx = 1 - dst_idx;
  compute_block_hashes(buf[src_idx], buf[dst_idx], sub_width);
}
```

Good:

```cpp
int src_idx = 0;
for (int sub_width = 4; sub_width <= block_size;
     sub_width *= 2, src_idx = !src_idx) {
  const int dst_idx = !src_idx;
  compute_block_hashes(buf[src_idx], buf[dst_idx], sub_width);
}
```
