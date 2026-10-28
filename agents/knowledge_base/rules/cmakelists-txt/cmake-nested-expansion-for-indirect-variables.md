---
name: libaom/cmakelists-txt/cmake-nested-expansion-for-indirect-variables
description: When dereferencing indirect CMake variable names in loops or macros, use nested expansion ${${var_name}} to inspect variable contents rather than the variable name
rubric: correctness
severity: should_fix
---

**Rule.** In CMake scripts, use nested variable expansion `${${var_name}}` when
dereferencing indirect variable names (such as iterating over a list of variable
identifiers in `foreach`) to inspect or manipulate the variable's value instead
of its name string.

**Why.** In CMake, `${var_name}` evaluates to the variable's identifier name
string rather than its stored value when `var_name` holds the name of another
variable. Accessing the underlying value requires nested expansion
`${${var_name}}` to avoid evaluating string commands or checks against variable
names.

## Examples

Bad:

```cpp
foreach(aom_source_var ${aom_source_vars})
  string(FIND "${aom_source_var}" "${AOM_ROOT}" aom_find_substring_index)
  if(NOT aom_find_substring_index EQUAL -1)
    file(APPEND "${libaom_srcs_gni_file}" "]\n")
  endif()
endforeach()
```

Good:

```cpp
foreach(aom_source_var ${aom_source_vars})
  string(FIND "${${aom_source_var}}" "${AOM_ROOT}" aom_find_substring_index)
  if(NOT aom_find_substring_index EQUAL -1)
    file(APPEND "${libaom_srcs_gni_file}" "]\n")
  endif()
endforeach()
```
