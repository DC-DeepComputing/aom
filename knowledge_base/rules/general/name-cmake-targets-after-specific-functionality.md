---
name: libaom/name-cmake-targets-after-specific-functionality
description: When defining CMake build targets, source variables, or setup functions, name them after their specific functionality rather than overly broad directory names
rubric: readability
severity: should_fix
---

**Rule.** Name CMake build targets, source variables, and setup functions after
their specific component or feature functionality rather than using broad
directory-level names (e.g., ivf_dec and IVF_DEC_SOURCES instead of aom_common
and AOM_COMMON_SOURCES) when they only encompass a specific subset of files in
that directory.

**Why.** Overly broad target or variable names misleadingly imply that the build
target compiles the entire directory or subsystem, creating confusion for
maintainers and hiding what functionality is actually included.

## Examples

Bad:

```cpp
# common/aom_common.cmake
list(APPEND AOM_COMMON_SOURCES
            "${AOM_ROOT}/common/ivfdec.c"
            "${AOM_ROOT}/common/ivfdec.h")

function(setup_aom_common_targets)
  add_library(aom_common OBJECT ${AOM_COMMON_SOURCES})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_common PARENT_SCOPE)
  target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_common>)
endfunction()
```

Good:

```cpp
# common/ivf_dec.cmake
list(APPEND IVF_DEC_SOURCES
            "${AOM_ROOT}/common/ivfdec.c"
            "${AOM_ROOT}/common/ivfdec.h")

function(setup_ivf_dec_targets)
  add_library(ivf_dec OBJECT ${IVF_DEC_SOURCES})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} ivf_dec PARENT_SCOPE)
  target_sources(aom PRIVATE $<TARGET_OBJECTS:ivf_dec>)
endfunction()
```
