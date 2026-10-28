---
name: libaom/prefer-zero-initialization-over-memset-in-cpp
description: When initializing local structs, aggregates, or POD variables in C++ code, use value-initialization (`Type var = {};`) at declaration instead of `memset`
rubric: readability
severity: nit
---

**Rule.** In C++ code, prefer value-initialization (`Type var = {};`) upon
declaration rather than declaring uninitialized POD or aggregate structs and
clearing them with `memset(&var, 0, sizeof(var))`. Initialize local structs and
configuration objects directly at declaration.

**Why.** Value-initialization (`= {}`) is idiomatic and concise in C++, avoids
separate declaration and assignment statements, eliminates boilerplate `memset`
and `sizeof` calls, and prevents bugs from passing incorrect size or pointer
arguments.

## Examples

Bad:

```cpp
aom_svc_params_t svc_params;
memset(&svc_params, 0, sizeof(svc_params));
```

Good:

```cpp
aom_svc_params_t svc_params = {};
```
