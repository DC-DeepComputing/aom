---
name: libaom/av1/update-row-mt-progress-counters-monotonically
description: When updating multithreaded row or pipeline progress counters in sync write functions, advance counters monotonically with AOMMAX to prevent overwriting error unblocking states
rubric: safety
severity: blocking
---

**Rule.** In multithreaded row or pipeline synchronization where progress
counters (such as num_finished_cols[r]) are set to a maximum value upon early
exit or error to unblock waiting threads, ensure progress counters are updated
monotonically using AOMMAX within the synchronization write routines rather than
by simple assignment or after unchecked/unlocked error flag sampling.

**Why.** If an erroring or early-exiting thread sets the row progress counter to
the maximum column count to unblock dependent workers, an unconditional
assignment in a concurrent worker's sync_write can overwrite the maximum value
with an earlier column index. This leaves downstream threads waiting
indefinitely in sync_read and deadlocks the pipeline.

## Examples

Bad:

```cpp
// In sync_write:
row_mt_sync->num_finished_cols[r] = cur;

// In worker thread:
pthread_mutex_lock(enc_row_mt->mutex_);
mb_wiener_mt_exit = enc_row_mt->mb_wiener_mt_exit;
pthread_mutex_unlock(enc_row_mt->mutex_);
if (!mb_wiener_mt_exit) {
  intra_mt->intra_sync_write_ptr(intra_row_mt_sync, mt_thread_id, mt_unit_col,
                                 mt_unit_cols);
}
```

Good:

```cpp
// In sync_write: ensure progress counter updates monotonically so early error unblocking is preserved
row_mt_sync->num_finished_cols[r] =
    AOMMAX(row_mt_sync->num_finished_cols[r], cur);

// In worker thread:
intra_mt->intra_sync_write_ptr(intra_row_mt_sync, mt_thread_id, mt_unit_col,
                               mt_unit_cols);
```
