---
name: libaom/reuse-existing-structs-and-helpers-instead-of-duplicates
description: When implementing encoder multi-threading or feature sync, reuse existing structs like AV1EncRowMultiThreadSync and lifecycle helpers rather than duplicating types and functions
rubric: design
severity: should_fix
---

**Rule.** Do not define duplicate or parallel struct types (such as creating
stage-specific sync structs with identical layout) or duplicate helper functions
across encoder stages or components. When implementing row multi-threading
synchronization, reuse existing synchronization structures (such as
AV1EncRowMultiThreadSync) and their associated lifecycle and synchronization
helpers (including row_mt_sync_mem_alloc(), av1_row_mt_sync_mem_dealloc(),
av1_row_mt_sync_read(), and av1_row_mt_sync_write()) by instantiating distinct
member variables or passing instances to shared helpers.

**Why.** Reimplementing duplicate struct definitions and lifecycle routines
(allocation, deallocation, read/write sync) creates redundant code, increases
maintenance overhead, risks logic divergence when bug fixes or mutex/condvar
initialization changes occur, and prevents sharing common row multi-threading
infrastructure across pipeline stages.

## Examples

Bad:

```cpp
// 1. Defining duplicate sync struct and read/write synchronization functions:
typedef struct {
  pthread_mutex_t *mutex_;
  pthread_cond_t *cond_;
  int *num_finished_cols;
  int sync_range;
  int rows;
} AV1EncAllIntraRowMultiThreadSync;

void av1_allintra_row_mt_sync_read(AV1EncAllIntraRowMultiThreadSync *sync, int r, int c) {
  // Duplicated synchronization logic...
}

// 2. Reimplementing custom allocation/deallocation routines for MT sync structures:
static void wiener_var_sync_mem_alloc(AV1EncRowMultiThreadSync *intra_row_mt_sync,
                                      AV1_COMMON *cm, int rows) {
  // Duplicated mutex/condvar allocation and initialization...
}
static void wiener_var_sync_mem_dealloc(AV1EncRowMultiThreadSync *intra_row_mt_sync) {
  // Duplicated deallocation logic...
}
```

Good:

```cpp
// 1. Reuse existing AV1EncRowMultiThreadSync type and shared sync read/write helpers:
AV1EncRowMultiThreadSync intra_row_mt_sync;
av1_row_mt_sync_read(&intra_row_mt_sync, r, c);
av1_row_mt_sync_write(&intra_row_mt_sync, r, c, cols);

// 2. Reuse standard row MT memory allocation and deallocation helpers:
row_mt_sync_mem_alloc(&cpi->ppi->intra_row_mt_sync, cm, mi_rows);
...
av1_row_mt_sync_mem_dealloc(&cpi->ppi->intra_row_mt_sync);
```
