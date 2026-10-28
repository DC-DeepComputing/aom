---
name: libaom/avoid-wclobbered-by-declaring-after-setjmp
description: When using setjmp for error handling, declare local variables and pointers after the setjmp block to prevent -Wclobbered warnings without volatile prototype churn
rubric: correctness
severity: should_fix
---

**Rule.** When writing functions that use setjmp() for error recovery, do not
declare local variables or pointers before setjmp() if they are only needed
after error initialization. Scope local declarations after the setjmp()
error-handling block rather than qualifying variables as volatile, preventing
GCC -Wclobbered warnings and avoiding cascading volatile parameter churn across
downstream function prototypes.

**Why.** GCC emits -Wclobbered on certain architectures (e.g. ARM) when
automatic variables declared before setjmp() are modified or live across longjmp
execution paths. Adding volatile to silence the warning forces downstream
function signatures that receive those pointers to also take volatile, creating
unnecessary prototype churn, whereas moving variable declarations after the
setjmp() block eliminates the warning cleanly.

## Examples

Bad:

```cpp
// Declaring pointers before setjmp() triggers -Wclobbered or forces volatile qualifiers.
AV1_COMMON *const cm = &cur_cpi->common;
cm->error = &error;
if (setjmp(error.jmp)) {
  error.setjmp = 0;
  cm->error = backup_cm_error;
  restore_workers_after_fpmt(ppi, parallel_frame_count, num_fpmt_workers_prepared);
  aom_internal_error_copy(&ppi->error, &error);
}
```

Good:

```cpp
// Declare pointers only after the setjmp() block if not needed in the recovery path.
struct aom_internal_error_info *const error = cur_cpi->common.error;
if (setjmp(error->jmp)) {
  error->setjmp = 0;
  restore_workers_after_fpmt(ppi, parallel_frame_count, i);
  aom_internal_error_copy(&ppi->error, error);
}
error->setjmp = 1;
AV1_COMMON *const cm = &cur_cpi->common;
```
