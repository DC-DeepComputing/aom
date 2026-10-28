---
name: libaom/keep-state-mutations-outside-multithread-guards
description: 'When synchronizing threads with #if CONFIG_MULTITHREAD, keep shared state mutations and exit flag assignments outside preprocessor guards so single-threaded builds execute them'
rubric: correctness
severity: should_fix
---

**Rule.** Do not place shared state mutations, error flags, or loop/worker exit
indicators inside '#if CONFIG_MULTITHREAD' preprocessor guards. Restrict '#if
CONFIG_MULTITHREAD' strictly to threading and synchronization primitives (such
as mutex locks, condition variables, and thread joins/spawns) so state updates
and exit signaling execute unconditionally in single-threaded builds
('CONFIG_MULTITHREAD=0').

**Why.** When libaom is compiled with multithreading disabled (such as
'-DCONFIG_MULTITHREAD=0' in Emscripten or single-threaded embedded builds), code
enclosed in '#if CONFIG_MULTITHREAD' blocks is excluded entirely by the
preprocessor. If state mutations, error transitions, or exit flags are placed
inside these guards, single-threaded builds will skip setting those flags,
leading to silent failures, unhandled errors, desynchronization, corrupted state
transitions, or infinite loops.

## Examples

Bad:

```cpp
#if CONFIG_MULTITHREAD
pthread_mutex_lock(pbi->row_mt_mutex_);
frame_row_mt_info->row_mt_exit = 1;
pthread_cond_broadcast(pbi->row_mt_cond_);
pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif
```

Good:

```cpp
#if CONFIG_MULTITHREAD
pthread_mutex_lock(pbi->row_mt_mutex_);
#endif
frame_row_mt_info->row_mt_exit = 1;
#if CONFIG_MULTITHREAD
pthread_cond_broadcast(pbi->row_mt_cond_);
pthread_mutex_unlock(pbi->row_mt_mutex_);
#endif
```
