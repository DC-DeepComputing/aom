---
name: libaom/clamp-buffer-pointer-advancement-to-end-boundary
description: When advancing buffer pointers or indices by variable offsets, clamp or validate the resulting position against the buffer end boundary to prevent out-of-bounds overruns
rubric: safety
severity: should_fix
---

**Rule.** When advancing buffer pointers, stream cursors, or array indices by
variable step sizes or block offsets (such as `ptr += offset`), validate or
clamp the resulting position against the buffer end pointer or limit (e.g.,
`buf_end`, `stats_in_end`). If an overrun is logically impossible due to caller
preconditions, explicitly verify the invariant with an assertion.

**Why.** Direct pointer arithmetic with variable offsets without bounds
validation can advance pointers past the allocated buffer end or
one-past-the-end limit when the offset exceeds remaining capacity. Clamping or
asserting against the end boundary prevents undefined pointer arithmetic and
out-of-bounds memory accesses.

## Examples

Bad:

```cpp
// Unchecked pointer arithmetic can push stats_in past stats_in_end.
cpi->twopass_frame.stats_in += new_i;
```

Good:

```cpp
// Advance pointer and clamp to buffer end boundary.
cpi->twopass_frame.stats_in += new_i;
if (cpi->twopass_frame.stats_in >= twopass->stats_buf_ctx->stats_in_end) {
  cpi->twopass_frame.stats_in = twopass->stats_buf_ctx->stats_in_end;
}
```
