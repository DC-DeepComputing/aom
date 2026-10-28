---
name: libaom/cmakelists-txt/consistent-cmake-scratch-variable-names
description: When performing string(FIND) or substring searches in CMake scripts, use consistent scratch variable names like aom_find_substring_index across operations
rubric: readability
severity: nit
---

**Rule.** In CMake build scripts, keep temporary output and scratch variable
names consistent across repeated operations (such as standardizing on
`aom_find_substring_index` for `string(FIND ...)` substring checks) rather than
introducing arbitrary ad-hoc variable names for each check.

**Why.** Using arbitrary one-off variable names for temporary output values adds
clutter and makes repetitive build-system checks harder to read and maintain.
Standardizing on a uniform scratch variable name keeps CMake scripts consistent
and concise.

## Examples

Bad:

```cpp
string(FIND "${file}" "${AOM_CONFIG_DIR}" aom_config_dir_match_index)
if(NOT aom_config_dir_match_index EQUAL -1)
  # ...
endif()
string(FIND "${file}" "${AOM_ROOT}" file_index)
if(NOT file_index EQUAL -1)
  # ...
endif()
```

Good:

```cpp
string(FIND "${file}" "${AOM_CONFIG_DIR}" aom_find_substring_index)
if(NOT aom_find_substring_index EQUAL -1)
  # ...
endif()
string(FIND "${file}" "${AOM_ROOT}" aom_find_substring_index)
if(NOT aom_find_substring_index EQUAL -1)
  # ...
endif()
```
