---
name: libaom/use-break-for-post-loop-cleanup
description: When terminating a single-level loop early in a function with post-loop cleanup, use break to reach epilogue cleanup instead of duplicating cleanup before return
rubric: readability
severity: should_fix
---

**Rule.** When exiting early from a single-level loop in a function that
contains post-loop epilogue or cleanup code, use break to exit the loop and
execute the shared cleanup sequence. Do not duplicate cleanup statements before
an early return, and avoid jumping to a cleanup label with goto when break
naturally flows into the post-loop cleanup.

**Why.** Using break to exit a single-level loop leverages the function's
existing trailing cleanup logic, eliminating duplicated teardown statements
across multiple return paths and avoiding unnecessary goto labels and jumps.

## Examples

Bad:

```cpp
for (int mi_col = 0; mi_col < mi_cols; mi_col += mb_step) {
  if (enc_row_mt->mb_wiener_mt_exit) {
    xd->mi = NULL;
    return;
  }
  // Process block...
}
xd->mi = NULL;
```

Good:

```cpp
for (int mi_col = 0; mi_col < mi_cols; mi_col += mb_step) {
  if (enc_row_mt->mb_wiener_mt_exit) {
    break;
  }
  // Process block...
}
xd->mi = NULL;
```
