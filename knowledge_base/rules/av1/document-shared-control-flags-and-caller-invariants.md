---
name: libaom/av1/document-shared-control-flags-and-caller-invariants
description: When adding control flags or conditional allocation logic in functions shared across encoder and decoder, document the flag's purpose and differing caller lifecycle assumptions
rubric: readability
severity: nit
---

**Rule.** When introducing or modifying control flags, mode parameters, or
conditional allocation and initialization branches in functions shared across
distinct contexts (such as encoder vs. decoder or single-threaded vs. FPMT /
multi-threaded worker pools), add an explanatory comment. Describe what the flag
controls, what values each caller passes, and any underlying resource ownership,
worker sharing, or lifecycle assumptions.

**Why.** Shared common functions often serve multiple callers with fundamentally
different execution environments and resource lifecycles (such as workers shared
across parallel frames during frame-parallel multithreading vs. independently
allocated per frame in decoding). Without comments explaining why a control flag
is bypassed or enabled for specific callers, maintainers cannot verify resource
invariants or safely update shared allocation paths.

## Examples

Bad:

```cpp
// In av1/common/alloccommon.c
int av1_alloc_cdef_buffers(AV1_COMMON *cm, AV1CdefWorkerData **cdef_worker,
                           AV1CdefSync *cdef_sync, int num_workers,
                           int init_worker) {
  if (*cdef_worker != NULL && init_worker) {
    if (is_num_workers_changed) {
      aom_free(*cdef_worker);
      *cdef_worker = NULL;
    }
  }
  // ...
```

Good:

```cpp
// In av1/common/alloccommon.c
int av1_alloc_cdef_buffers(AV1_COMMON *cm, AV1CdefWorkerData **cdef_worker,
                           AV1CdefSync *cdef_sync, int num_workers,
                           int init_worker) {
  // The flag init_worker indicates if cdef_worker has to be allocated for the
  // frame. This is passed as 1 always from decoder. At encoder side, it is 0
  // when called for parallel frames during FPMT (where cdef_worker is shared
  // across parallel frames) and 1 otherwise.
  if (*cdef_worker != NULL && init_worker) {
    if (is_num_workers_changed) {
      aom_free(*cdef_worker);
      *cdef_worker = NULL;
    }
  }
  // ...
```
