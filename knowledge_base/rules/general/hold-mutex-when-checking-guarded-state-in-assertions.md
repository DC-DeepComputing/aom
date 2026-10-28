---
name: libaom/hold-mutex-when-checking-guarded-state-in-assertions
description: When checking mutex-guarded shared state in assertions, acquire the guarding mutex or call synchronized predicate helpers to prevent data races
rubric: safety
severity: should_fix
---

**Rule.** Do not read or check shared fields guarded by a mutex—even inside
debug assertions (`assert` or `aom_assert`)—without acquiring the guarding mutex
or calling a synchronized helper. Accessing shared variables outside of mutex
protection constitutes a data race against concurrent writers and triggers
ThreadSanitizer (TSan) violations.

**Why.** Reading shared memory outside of synchronization primitives violates
C/C++ memory model invariants and races against concurrent threads modifying the
state. Even in debug-only checks, unsynchronized reads produce undefined
behavior and ThreadSanitizer (TSan) data-race warnings.

## Examples

Bad:

```cpp
// Reading mutex-guarded state directly in assertions without locking creates a data race:
assert(src_pyramid->valid);
```

Good:

```cpp
// Acquire the mutex when inspecting guarded fields, or call a synchronized helper:
#if CONFIG_MULTITHREAD
pthread_mutex_lock(&src_pyramid->mutex);
#endif
assert(src_pyramid->valid);
#if CONFIG_MULTITHREAD
pthread_mutex_unlock(&src_pyramid->mutex);
#endif

// Or encapsulate the check in a thread-safe helper function:
assert(aom_is_pyramid_valid(src_pyramid, n_levels));
```
