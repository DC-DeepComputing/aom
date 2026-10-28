---
name: libaom/avoid-sequential-check-mem-error-leaks
description: When allocating multiple dynamic buffers in the same scope, avoid sequential CHECK_MEM_ERROR macros that leak earlier allocations on failure; free buffers before erroring
rubric: safety
severity: should_fix
---

**Rule.** Do not use sequential CHECK_MEM_ERROR() or AOM_CHECK_MEM_ERROR()
macros to allocate multiple heap buffers in the same scope. Instead, perform
allocations separately, check if any allocation returned NULL, and explicitly
free all allocated non-null buffers before calling aom_internal_error().

**Why.** CHECK_MEM_ERROR() invokes aom_internal_error(), which performs a
longjmp out of the function on allocation failure without unwinding local
variables. When multiple allocations are executed sequentially with
CHECK_MEM_ERROR(), a failure in any allocation after the first will immediately
longjmp, leaking all previously allocated buffers.

## Examples

Bad:

```cpp
// Bad: If vbuf or src_hbuf allocation fails, CHECK_MEM_ERROR longjmps,
// leaking hbuf and any prior successfully allocated buffers.
CHECK_MEM_ERROR(cm, hbuf,
                (int16_t *)aom_malloc(search_width * sizeof(*hbuf)));
CHECK_MEM_ERROR(cm, vbuf,
                (int16_t *)aom_malloc(search_height * sizeof(*vbuf)));
CHECK_MEM_ERROR(cm, src_hbuf, (int16_t *)aom_malloc(bw * sizeof(*src_hbuf)));
CHECK_MEM_ERROR(cm, src_vbuf, (int16_t *)aom_malloc(bh * sizeof(*src_vbuf)));
```

Good:

```cpp
// Good: Allocate buffers and check for failure together, freeing all
// allocated buffers before raising the internal error.
int16_t *hbuf = (int16_t *)aom_malloc(search_width * sizeof(*hbuf));
int16_t *vbuf = (int16_t *)aom_malloc(search_height * sizeof(*vbuf));
int16_t *src_hbuf = (int16_t *)aom_malloc(bw * sizeof(*src_hbuf));
int16_t *src_vbuf = (int16_t *)aom_malloc(bh * sizeof(*src_vbuf));
if (!hbuf || !vbuf || !src_hbuf || !src_vbuf) {
  aom_free(hbuf);
  aom_free(vbuf);
  aom_free(src_hbuf);
  aom_free(src_vbuf);
  aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                     "Failed to allocate memory");
}
```
