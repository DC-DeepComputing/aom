---
name: libaom/avoid-transitive-windows-header-pollution
description: When including platform or threading headers in shared .h files, avoid transitively pulling in <windows.h> and leaking macros that collide with common identifiers
rubric: correctness
severity: blocking
---

**Rule.** Do not transitively include platform headers like <windows.h> (e.g.,
via aom_pthread.h or threading wrappers) in widely shared header files, and
avoid defining identifiers that collide with Windows SDK macros (such as s_lh,
min, max, or ERROR). Isolate platform-specific headers and synchronization types
in source files or dedicated wrapper implementation files whenever possible.

**Why.** Windows SDK headers pollute the global preprocessor namespace with
hundreds of legacy macro definitions. When included transitively through shared
headers, these macros silently clobber local variable names, struct members, and
helper functions across downstream translation units on MSVC/Windows builds.

## Examples

Bad:

```cpp
// In a shared header (e.g., aom_dsp/pyramid.h):
#include "aom_util/aom_pthread.h"  // Pulls in <windows.h> on Windows

struct ImagePyramid {
  pthread_mutex_t mutex;
  int s_lh;  // Clashes with Windows SDK macro '#define s_lh ...'
};
```

Good:

```cpp
// In a shared header, forward-declare or isolate thread structs in .c files,
// or avoid identifier names known to collide with Windows SDK macros:
struct ImagePyramid {
  void *mutex;  // Or include aom_pthread.h only in pyramid.c
  int sub_lh;   // Renamed to avoid Windows macro collision
};
```
