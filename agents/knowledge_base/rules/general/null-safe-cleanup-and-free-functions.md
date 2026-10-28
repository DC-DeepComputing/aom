---
name: libaom/null-safe-cleanup-and-free-functions
description: When implementing resource cleanup, teardown, or deallocation functions (such as *_free or *_destroy), accept NULL pointers as a safe no-op before dereferencing.
rubric: readability
severity: nit
---

**Rule.** Ensure public, internal, and helper resource cleanup or deallocation
functions (such as *_free(), *_destroy(), or *_close()) accept NULL or nullptr
pointers gracefully as a safe no-op. Validate input pointers at function entry
and return early before accessing fields or freeing nested resources.

**Why.** Standard C free() semantics treat NULL as a no-op. Ensuring cleanup and
destruction helpers accept NULL pointers prevents crashes during defensive
cleanup, partial initialization handling, or error unwinding paths where
pointers may be uninitialized or already freed.

## Examples

Bad:

```cpp
void obudec_free(struct ObuDecInputContext *obu_ctx) {
  // Crashes if obu_ctx is NULL
  free(obu_ctx->buffer);
}
```

Good:

```cpp
void obudec_free(struct ObuDecInputContext *obu_ctx) {
  if (!obu_ctx) return;
  free(obu_ctx->buffer);
  memset(obu_ctx, 0, sizeof(*obu_ctx));
}
```
