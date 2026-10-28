---
name: libaom/cmakelists-txt/avoid-cmake-matches-for-path-substrings
description: When checking if a file path contains a directory or substring in CMake, use string(FIND ...) instead of if(... MATCHES ...)
rubric: correctness
severity: blocking
---

**Rule.** In CMake build scripts, do not use `if(... MATCHES ...)` or
`string(REGEX MATCH ...)` to test whether a file path contains a directory
variable or substring; use `string(FIND ... <index_var>)` and check whether the
index is `-1` instead.

**Why.** CMake's `MATCHES` operator treats the right-hand operand as a regular
expression. When checking whether a path contains a directory variable or prefix
(such as `${AOM_CONFIG_DIR}` or `${AOM_ROOT}`), paths containing regex
metacharacters such as `+` (common in Bazel execution paths or custom build
directory names) cause CMake to fail with regular expression compilation errors.
Using `string(FIND ...)` performs an exact substring search without regex
parsing.

## Examples

Bad:

```cpp
if(NOT "${file}" MATCHES "${AOM_CONFIG_DIR}")
  list(APPEND aom_sources "${file}")
endif()
```

Good:

```cpp
string(FIND "${file}" "${AOM_CONFIG_DIR}" config_dir_index)
if(config_dir_index EQUAL -1)
  list(APPEND aom_sources "${file}")
endif()
```
