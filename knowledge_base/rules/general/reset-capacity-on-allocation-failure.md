---
name: libaom/reset-capacity-on-allocation-failure
description: When allocating or resizing struct buffers, update capacity and size fields only after allocation succeeds or reset them on failure, avoiding unrelated struct field resets
rubric: correctness
severity: should_fix
---

**Rule.** When allocating memory or resizing dynamically allocated struct
buffers (using malloc, realloc, calloc, or aom_malloc), maintain struct
consistency: update capacity, storage, and size tracking members only after the
allocation succeeds, or explicitly reset them to zero if the allocation fails
(buffer == NULL). Never leave a struct with non-zero capacity or size fields
when its buffer pointer is NULL. Conversely, do not perform arbitrary or
unrelated struct member resets (such as clearing offsets or clobbering existing
valid buffer state) on fatal or realloc failure paths before reporting errors
with aom_internal_error().

**Why.** Leaving a non-zero capacity or size field when a buffer pointer is NULL
or when reallocation fails creates an inconsistent struct state. Subsequent
operations or cleanup routines that inspect capacity and assume a valid buffer
will trigger null-pointer dereferences or memory corruption. Conversely,
performing unrelated struct member resets on fatal error paths or failed realloc
calls introduces misleading mutations and clobbers valid existing state prior to
error handling.

## Examples

Bad:

```cpp
// Bad: Pre-updating capacity leaves inconsistent non-zero size on allocation failure.
accounting->num_syms_allocated = 1000;
accounting->syms.syms =
    malloc(sizeof(AccountingSymbol) * accounting->num_syms_allocated);
if (!accounting->syms.syms) return false;

enc->buf = (unsigned char *)malloc(sizeof(*enc->buf) * size);
enc->storage = size;
if (size > 0 && enc->buf == NULL) {
  aom_internal_error(enc->error_info, AOM_CODEC_MEM_ERROR, "Out of memory");
}

// Bad: Realloc failure performs unrelated struct member reset.
out = (unsigned char *)realloc(out, sizeof(*out) * storage);
if (out == NULL) {
  enc->offs = 0;
  aom_internal_error(enc->error_info, AOM_CODEC_MEM_ERROR, "Out of memory");
}
```

Good:

```cpp
// Good: Reset capacity/storage to 0 (or defer assignment) when allocation fails.
accounting->num_syms_allocated = 1000;
accounting->syms.syms =
    malloc(sizeof(AccountingSymbol) * accounting->num_syms_allocated);
if (!accounting->syms.syms) {
  accounting->num_syms_allocated = 0;
  return false;
}

enc->buf = (unsigned char *)malloc(sizeof(*enc->buf) * size);
if (size > 0 && enc->buf == NULL) {
  enc->storage = 0;
  aom_internal_error(enc->error_info, AOM_CODEC_MEM_ERROR, "Out of memory");
}
enc->storage = size;

// Good: Realloc failure preserves existing struct state and reports error.
out = (unsigned char *)realloc(out, sizeof(*out) * storage);
if (out == NULL) {
  aom_internal_error(enc->error_info, AOM_CODEC_MEM_ERROR, "Out of memory");
}
```
