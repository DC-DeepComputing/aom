---
name: libaom/return-codec-ok-after-successful-setjmp
description: When returning from functions after a setjmp error-handling block, return AOM_CODEC_OK explicitly on success rather than returning or checking error->error_code
rubric: correctness
severity: should_fix
---

**Rule.** Do not check or return `error->error_code` from `struct
aom_internal_error_info` on the normal execution path following a `setjmp`
block; return `AOM_CODEC_OK` explicitly on success. `error->error_code` is only
populated and meaningful when `aom_internal_error()` triggers a `longjmp()`, and
is not reset on normal execution.

**Why.** Like `errno`, `aom_internal_error_info::error_code` is only valid when
an error occurs and triggers `longjmp()`. On the normal, non-exceptional path
following a `setjmp` block, `error_code` is not reset to `AOM_CODEC_OK` and may
contain a stale error code from a prior failure.

## Examples

Bad:

```cpp
if (setjmp(error->jmp)) {
  error->setjmp = 0;
  return error->error_code;
}
error->setjmp = 1;
av1_set_reference_dec(&frame_worker_data->pbi->common, frame->idx,
                      frame->use_external_ref, &sd);
error->setjmp = 0;
return error->error_code;
```

Good:

```cpp
if (setjmp(error->jmp)) {
  error->setjmp = 0;
  return error->error_code;
}
error->setjmp = 1;
av1_set_reference_dec(&frame_worker_data->pbi->common, frame->idx,
                      frame->use_external_ref, &sd);
error->setjmp = 0;
return AOM_CODEC_OK;
```
