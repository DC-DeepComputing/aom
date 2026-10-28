---
name: libaom/av1/check-exit-flag-in-condition-variable-wait-loops
description: In multi-threaded condition variable wait loops, check termination or early-exit flags directly in the wait predicate rather than mutating progress counters to force unblocking
rubric: safety
severity: should_fix
---

**Rule.** In multi-threaded condition variable wait loops, include early-exit
and error termination flags directly in the pthread_cond_wait loop predicate
(and broadcast the condition variable when signalling exit), rather than
mutating progress counters or completion states to force unblocking. After
exiting the wait loop, check the exit flag and return or abort promptly.

**Why.** Mutating task progress counters or falsely marking dependent rows/tiles
as completed to wake sleeping threads can cause race conditions or corrupt
shared state. Checking an explicit exit flag in the wait predicate allows worker
threads to wake up and terminate cleanly upon notification.

## Examples

Bad:

```cpp
// Worker wait loop:
pthread_mutex_lock(enc_row_mt_mutex_);
while (enc_row_mt->num_tile_cols_done[cur_sb_row] < cm->tiles.cols ||
       enc_row_mt->num_tile_cols_done[next_sb_row] < cm->tiles.cols) {
  pthread_cond_wait(enc_row_mt->cond_, enc_row_mt_mutex_);
}
pthread_mutex_unlock(enc_row_mt_mutex_);

// On error handler:
memset(enc_row_mt->num_tile_cols_done, cm->tiles.cols,
       sizeof(*enc_row_mt->num_tile_cols_done) * sb_rows_in_frame);
pthread_cond_broadcast(enc_row_mt->cond_);
```

Good:

```cpp
// Worker wait loop:
pthread_mutex_lock(enc_row_mt_mutex_);
while (!enc_row_mt->row_mt_exit &&
       (enc_row_mt->num_tile_cols_done[cur_sb_row] < cm->tiles.cols ||
        enc_row_mt->num_tile_cols_done[next_sb_row] < cm->tiles.cols)) {
  pthread_cond_wait(enc_row_mt->cond_, enc_row_mt_mutex_);
}
const bool row_mt_exit = enc_row_mt->row_mt_exit;
pthread_mutex_unlock(enc_row_mt_mutex_);
if (row_mt_exit) return;

// On error handler:
enc_row_mt->row_mt_exit = true;
pthread_cond_broadcast(enc_row_mt->cond_);
```
