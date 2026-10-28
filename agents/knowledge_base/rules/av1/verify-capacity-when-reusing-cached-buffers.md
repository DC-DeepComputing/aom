---
name: libaom/av1/verify-capacity-when-reusing-cached-buffers
description: When allocating or reusing cached memory buffers across invocations or frames, check both whether the pointer is NULL and whether existing capacity is insufficient before resizing
rubric: correctness
severity: should_fix
---

**Rule.** When managing reusable or cached memory buffers across frames or
function calls, verify both that the buffer is allocated (`buf != NULL`) and
that its existing capacity is sufficient (`buf_sz >= required_sz`) before use.
If the required size exceeds the current capacity or the buffer is unallocated
(`buf == NULL || buf_sz < required_sz`), free any previously allocated buffer
and allocate the required size (or use `realloc`), updating the recorded
capacity accordingly.

**Why.** Checking only whether the buffer pointer is NULL leaves previously
allocated buffers undersized when required data sizes increase across frames or
invocations, leading to heap buffer overflows or memory corruption when writing
data.

## Examples

Bad:

```cpp
// Anti-pattern: Only checks for NULL, leaving buffer undersized if data_sz grows
if (ctx->cx_data == NULL) {
  ctx->cx_data_sz = data_sz;
  ctx->cx_data = (unsigned char *)malloc(ctx->cx_data_sz);
}
```

Good:

```cpp
// Correct: Reallocates when NULL or when existing capacity is insufficient
if (ctx->cx_data == NULL || ctx->cx_data_sz < data_sz) {
  ctx->cx_data_sz = data_sz;
  free(ctx->cx_data);
  ctx->cx_data = (unsigned char *)malloc(ctx->cx_data_sz);
  if (ctx->cx_data == NULL) {
    ctx->cx_data_sz = 0;
  }
}
```
