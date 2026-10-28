---
name: libaom/cmakelists-txt/document-compiler-warning-suppressions-in-build-files
description: When suppressing compiler warnings like `-Wno-*` in CMake build files, add an explanatory comment detailing why the warning occurs and why suppression is necessary
rubric: readability
severity: nit
---

**Rule.** When suppressing compiler warnings (such as via `-Wno-*` or `/wd*`
flags) for specific targets, third-party libraries, or global configurations in
CMake files, add an explanatory comment above the suppression explaining why the
warning is triggered and why disabling it is necessary.

**Why.** Suppressing compiler warnings can mask legitimate defects, regressions,
or unaddressed upstream issues. An explanatory comment documents the technical
rationale for the workaround, preserves context for future maintainers, and
clarifies under what conditions the suppression might safely be removed.

## Examples

Bad:

```cpp
is_flag_present(AOM_CXX_FLAGS "-Wmissing-prototypes" flag_present)
if(flag_present)
  target_compile_options(yuv PRIVATE -Wno-missing-prototypes)
endif()
```

Good:

```cpp
# Many functions in libyuv trigger this warning when enabled with clang.
is_flag_present(AOM_CXX_FLAGS "-Wmissing-prototypes" flag_present)
if(flag_present)
  target_compile_options(yuv PRIVATE -Wno-missing-prototypes)
endif()
```
