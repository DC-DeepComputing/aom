---
name: libaom/no-uninitialized-error-info-in-aom-check-mem-error
description: When checking memory allocations outside an initialized codec setjmp context, do not use AOM_CHECK_MEM_ERROR; check allocation pointers directly instead.
rubric: correctness
severity: should_fix
---

**Rule.** Do not use AOM_CHECK_MEM_ERROR with a local or uninitialized struct
aom_internal_error_info. When allocating memory in utility, DSP, or standalone
functions that do not operate within an initialized codec error context with an
active setjmp buffer, check allocation pointers directly against NULL and handle
failure explicitly.

**Why.** AOM_CHECK_MEM_ERROR() invokes aom_internal_error(), which asserts and
relies on info->setjmp and info->jmp to perform a non-local jump via longjmp().
An uninitialized local error struct contains indeterminate stack memory, causing
assertion failures in debug builds or undefined behavior and silent continuation
on memory allocation failure in release builds.

## Examples

Bad:

```cpp
struct aom_internal_error_info error;
uint8_t *src_rgb = NULL;
uint8_t *distorted_rgb = NULL;
AOM_CHECK_MEM_ERROR(&error, src_rgb, aom_malloc(buffer_size));
AOM_CHECK_MEM_ERROR(&error, distorted_rgb, aom_malloc(buffer_size));
```

Good:

```cpp
uint8_t *src_rgb = (uint8_t *)aom_malloc(buffer_size);
uint8_t *distorted_rgb = (uint8_t *)aom_malloc(buffer_size);
if (!src_rgb || !distorted_rgb) {
  aom_free(src_rgb);
  aom_free(distorted_rgb);
  return AOM_CODEC_MEM_ERROR;
}
```
