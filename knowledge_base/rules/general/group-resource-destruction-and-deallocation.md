---
name: libaom/group-resource-destruction-and-deallocation
description: When releasing dynamically allocated resources that require explicit destruction, group the memory deallocation call inside the non-NULL check alongside the destruction call
rubric: readability
severity: nit
---

**Rule.** When releasing dynamically allocated resources that require both
object destruction and memory deallocation (such as pthread mutexes or condition
variables), group the memory deallocation call (e.g. aom_free()) inside the same
non-NULL check block as the destruction call (e.g. pthread_mutex_destroy()). Do
not place deallocation calls outside the NULL check as separate unconditional
statements.

**Why.** Grouping object destruction and memory deallocation together inside the
non-NULL guard keeps cleanup cohesive and avoids superfluous deallocation calls
on NULL pointers when resource creation failed or was skipped.

## Examples

Bad:

```cpp
// Anti-pattern: destruction guarded by NULL check, but deallocation called outside
if (mt_sync->mutex_ != NULL)
  pthread_mutex_destroy(mt_sync->mutex_);
if (mt_sync->cond_ != NULL)
  pthread_cond_destroy(mt_sync->cond_);
aom_free(mt_sync->mutex_);
aom_free(mt_sync->cond_);
```

Good:

```cpp
// Group destruction and deallocation inside the NULL check block
if (mt_sync->mutex_ != NULL) {
  pthread_mutex_destroy(mt_sync->mutex_);
  aom_free(mt_sync->mutex_);
}
if (mt_sync->cond_ != NULL) {
  pthread_cond_destroy(mt_sync->cond_);
  aom_free(mt_sync->cond_);
}
```
