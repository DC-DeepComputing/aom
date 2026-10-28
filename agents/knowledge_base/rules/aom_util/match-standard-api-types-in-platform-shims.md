---
name: libaom/aom_util/match-standard-api-types-in-platform-shims
description: When implementing platform compatibility shims or emulation wrappers for standard APIs, declare parameter types matching standard specifications rather than generic void pointers
rubric: correctness
severity: should_fix
---

**Rule.** When writing platform compatibility shims or emulation wrappers for
standard APIs (such as POSIX threading or OS primitives), ensure function
signatures use standard parameter types and typedefs (such as 'const
pthread_attr_t *attr' for pthread_create) rather than generic 'const void *' or
untyped pointers. Define required standard typedefs or placeholder structs when
compiling on platforms where native headers are absent.

**Why.** Declaring shims with standard types preserves compile-time type safety
across all build configurations, prevents callers from silently passing
incompatible arguments, and ensures portable call sites.

## Examples

Bad:

```cpp
// Windows emulation shim uses generic void pointer for attributes
static INLINE int pthread_create(pthread_t *const thread,
                                 const void *attr,
                                 unsigned int(__stdcall *start)(void *),
                                 void *arg) {
  // ...
}
```

Good:

```cpp
typedef int pthread_attr_t;

// Signature matches standard POSIX specification for type safety
static INLINE int pthread_create(pthread_t *const thread,
                                 const pthread_attr_t *attr,
                                 unsigned int(__stdcall *start)(void *),
                                 void *arg) {
  // ...
}
```
