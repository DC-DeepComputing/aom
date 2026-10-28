---
name: libaom/av1/check-lookahead-peek-non-null
description: When querying lookahead buffer frames using av1_lookahead_peek or av1_lookahead_pop, verify returned lookahead_entry pointers are non-NULL before dereferencing their fields
rubric: correctness
severity: blocking
---

**Rule.** Always verify that pointers returned by av1_lookahead_peek() or
av1_lookahead_pop() are non-NULL before accessing their fields (such as
entry->img). Do not dereference returned lookahead entries without validating
that an entry exists at the queried index.

**Why.** av1_lookahead_peek() returns NULL when no buffer exists at the
specified lookahead index (such as near sequence boundaries, during queue
draining, or when depth is insufficient). Accessing fields on an unvalidated
pointer causes null pointer dereferences and segmentation faults.

## Examples

Bad:

```cpp
struct lookahead_entry *e =
    av1_lookahead_peek(cpi->ppi->lookahead, i, cpi->compressor_stage);
struct lookahead_entry *e_prev =
    av1_lookahead_peek(cpi->ppi->lookahead, i - 1, cpi->compressor_stage);
frame_input.source = &e->img;
frame_input.last_source = &e_prev->img;
```

Good:

```cpp
struct lookahead_entry *e =
    av1_lookahead_peek(cpi->ppi->lookahead, i, cpi->compressor_stage);
struct lookahead_entry *e_prev =
    av1_lookahead_peek(cpi->ppi->lookahead, i - 1, cpi->compressor_stage);
if (e != NULL && e_prev != NULL) {
  frame_input.source = &e->img;
  frame_input.last_source = &e_prev->img;
}
```
