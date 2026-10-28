---
name: libaom/nullify-freed-in-out-pointer-on-allocation-failure
description: When freeing a resource referenced by an in-out pointer before reallocation, set the output pointer to NULL upon allocation failure before returning
rubric: safety
severity: should_fix
---

**Rule.** When freeing an existing heap resource or image buffer referenced by
an in-out pointer parameter (`T **out_ptr`) before attempting reallocation, set
the dereferenced output pointer (`*out_ptr = NULL` or `*out_ptr = nullptr`) upon
allocation failure before returning an error status. Ensure that failure paths
never return with the caller's pointer referencing freed memory.

**Why.** If an existing buffer is freed and a subsequent allocation fails,
returning failure without clearing the in-out pointer leaves the caller holding
a dangling pointer. When the caller executes its own cleanup or error-handling
paths (such as `if (buf) free(buf);`), it will attempt to free the already-freed
pointer, causing a double-free crash or memory corruption.

## Examples

Bad:

```cpp
int reallocate_buffer(size_t new_size, uint8_t **buf_ptr) {
  uint8_t *buf = *buf_ptr;
  if (buf && needs_realloc(buf, new_size)) {
    aom_free(buf);
    buf = NULL;
  }
  if (!buf) {
    buf = (uint8_t *)aom_malloc(new_size);
    if (!buf) return 0;  // *buf_ptr still points to the freed buffer!
  }
  *buf_ptr = buf;
  return 1;
}
```

Good:

```cpp
int reallocate_buffer(size_t new_size, uint8_t **buf_ptr) {
  uint8_t *buf = *buf_ptr;
  if (buf && needs_realloc(buf, new_size)) {
    aom_free(buf);
    buf = NULL;
  }
  if (!buf) {
    buf = (uint8_t *)aom_malloc(new_size);
    if (!buf) {
      *buf_ptr = NULL;  // Prevent caller from retaining dangling pointer
      return 0;
    }
  }
  *buf_ptr = buf;
  return 1;
}
```
