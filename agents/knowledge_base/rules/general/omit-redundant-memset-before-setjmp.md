---
name: libaom/omit-redundant-memset-before-setjmp
description: When setting up an aom_internal_error_info structure for setjmp(), omit redundant memset or zero-initialization calls prior to setjmp()
rubric: readability
severity: nit
---

**Rule.** Do not zero-initialize or call memset() on an aom_internal_error_info
structure before calling setjmp(). setjmp() initializes the jump buffer, the
setjmp flag is explicitly assigned immediately afterwards, and error detail
fields are only populated on failure via aom_internal_error() or
aom_set_error().

**Why.** Zero-initializing an aom_internal_error_info structure via memset()
before setjmp() is redundant because all relevant fields are either populated by
setjmp(), assigned explicitly, or written only when an error occurs. Omitting
the redundant memset eliminates dead stores and avoids unnecessary operations.

## Examples

Bad:

```cpp
struct aom_internal_error_info error_info;
memset(&error_info, 0, sizeof(error_info));
if (setjmp(error_info.jmp)) {
  error_info.setjmp = 0;
  return AOM_CODEC_MEM_ERROR;
}
error_info.setjmp = 1;
```

Good:

```cpp
struct aom_internal_error_info error_info;
if (setjmp(error_info.jmp)) {
  error_info.setjmp = 0;
  return AOM_CODEC_MEM_ERROR;
}
error_info.setjmp = 1;
```
