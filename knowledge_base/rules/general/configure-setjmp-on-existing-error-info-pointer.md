---
name: libaom/configure-setjmp-on-existing-error-info-pointer
description: When setting up setjmp error handling, bind setjmp directly to context error pointers without stack struct swapping, avoid returning error_info structs, and propagate failures using aom_internal_error_copy()
rubric: design
severity: should_fix
---

**Rule.** When establishing a local setjmp error-handling target for a codec
context (such as AV1_COMMON *cm or cur_cpi->common), invoke setjmp() directly on
the existing error->jmp pointer (e.g., cm->error or cur_cpi->common.error) and
toggle error->setjmp. Do not allocate a temporary struct aom_internal_error_info
on the stack and swap member pointers. Never return struct
aom_internal_error_info (by value or pointer) from internal functions. In the if
(setjmp(...)) recovery block, reset error->setjmp = 0, and propagate errors
across context or thread boundaries (such as to &ppi->error) using
aom_internal_error_copy() rather than returning error structs or discarding
formatted error messages.

**Why.** Codec contexts already provide dedicated, initialized
aom_internal_error_info pointers. Allocating temporary stack error structs and
swapping pointers risks dangling pointers if error unwinding or early returns
bypass pointer restoration. Furthermore, struct aom_internal_error_info
encapsulates a jmp_buf buffer designed for in-place error management, so
returning it by value copies jmp_buf state unnecessarily. Using
aom_internal_error_copy() preserves the full error code and diagnostic message
across context boundaries cleanly.

## Examples

Bad:

```cpp
// Bad: Allocating temporary stack error struct, swapping pointers, or returning error_info
struct aom_internal_error_info error;
struct aom_internal_error_info *backup_cm_error = cm->error;
if (setjmp(error.jmp)) {
  error.setjmp = 0;
  cm->error = backup_cm_error;
  restore_workers_after_fpmt(ppi, parallel_frame_count);
  return AOM_CODEC_MEM_ERROR;  // Discards specific error message
}
error.setjmp = 1;
cm->error = &error;

// Bad: Returning struct aom_internal_error_info by value
static struct aom_internal_error_info prepare_workers(...) {
  if (setjmp(error->jmp)) return *error;
}
```

Good:

```cpp
// Good: Bind setjmp directly to context error pointer and propagate with aom_internal_error_copy
static void prepare_workers(AV1_PRIMARY *ppi, struct aom_internal_error_info *error) {
  if (setjmp(error->jmp)) {
    error->setjmp = 0;
    restore_workers_after_fpmt(ppi, parallel_frame_count, num_workers);
    aom_internal_error_copy(&ppi->error, error);
    return;
  }
  error->setjmp = 1;
  // ... perform fallible work ...
  error->setjmp = 0;
}
```
