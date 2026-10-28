---
name: libaom/av1/reset-worker-exit-flags-before-dispatch
description: When dispatching multi-threaded workers, reset worker exit flags (such as *_mt_exit) unconditionally in dispatch routines rather than inside conditional memory allocation functions
rubric: correctness
severity: should_fix
---

**Rule.** When initializing or dispatching multi-threaded workers, reset worker
exit and error termination flags (such as row_mt_exit, tpl_mt_exit,
firstpass_mt_exit, or other *_mt_exit flags) unconditionally in the worker
dispatch or launch function before executing workers. Do not reset exit flags
inside conditional memory allocation routines (*_mem_alloc), which are skipped
on subsequent calls when buffers are reused.

**Why.** Memory allocation routines execute conditionally only when buffer
capacities change, whereas subsequent encode or decode calls reuse existing
allocations. If an earlier frame or pass aborts due to an error and leaves an
exit flag set to true, omitting the reset on subsequent calls will cause worker
threads to immediately terminate without executing their tasks.

## Examples

Bad:

```cpp
static void row_mt_mem_alloc(AV1_COMP *cpi, ...) {
  // ...
  // BAD: Resetting exit flags inside conditional memory allocation
  // leaves them stale when allocations are reused across frames.
  enc_row_mt->row_mt_exit = false;
  enc_row_mt->firstpass_mt_exit = false;
}

void av1_encode_tiles_row_mt(AV1_COMP *cpi) {
  if (alloc_row_mt_mem) {
    row_mt_mem_alloc(cpi, ...);
  }
  // Workers dispatched with stale row_mt_exit if allocation skipped!
  launch_workers(cpi);
}
```

Good:

```cpp
static void row_mt_mem_alloc(AV1_COMP *cpi, ...) {
  // ... allocate or resize memory buffers only ...
}

void av1_encode_tiles_row_mt(AV1_COMP *cpi) {
  if (alloc_row_mt_mem) {
    row_mt_mem_alloc(cpi, ...);
  }
  // GOOD: Unconditionally reset exit flag before dispatching workers.
  enc_row_mt->row_mt_exit = false;
  launch_workers(cpi);
}
```
