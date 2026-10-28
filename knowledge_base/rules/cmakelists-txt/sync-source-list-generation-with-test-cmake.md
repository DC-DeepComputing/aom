---
name: libaom/cmakelists-txt/sync-source-list-generation-with-test-cmake
description: When updating source list generation or path filtering for library targets in CMakeLists.txt, update the parallel test source list generation in test/test.cmake
rubric: correctness
severity: blocking
---

**Rule.** When modifying source list export, path normalization, or directory
filtering logic for library targets in CMakeLists.txt, update the corresponding
test source list generation logic in test/test.cmake (and vice versa) to ensure
both .txt and .gni files remain consistent.

**Why.** CMakeLists.txt and test/test.cmake maintain parallel source list
generation logic that exports libaom_srcs.{txt,gni} and
libaom_test_srcs.{txt,gni} for external build systems like GN. Changing path
filtering or string manipulation in only one file causes inconsistent source
paths or missing build targets.

## Examples

Bad:

```cpp
# In CMakeLists.txt (updated path matching logic):
string(FIND "${file}" "${AOM_CONFIG_DIR}" aom_config_dir_match_index)
if(aom_config_dir_match_index EQUAL -1)
  file(APPEND "${libaom_srcs_txt_file}" "${file}\n")
endif()

# But test/test.cmake was left with legacy or incompatible matching:
if(NOT "${file}" MATCHES "${AOM_CONFIG_DIR}")
  file(APPEND "${libaom_test_srcs_txt_file}" "${file}\n")
endif()
```

Good:

```cpp
# In CMakeLists.txt and test/test.cmake (updated consistently):
string(FIND "${file}" "${AOM_CONFIG_DIR}" aom_config_dir_match_index)
if(aom_config_dir_match_index EQUAL -1)
  file(APPEND "${libaom_srcs_txt_file}" "${file}\n")
endif()

# In test/test.cmake:
string(FIND "${file}" "${AOM_CONFIG_DIR}" aom_config_dir_match_index)
if(aom_config_dir_match_index EQUAL -1)
  file(APPEND "${libaom_test_srcs_txt_file}" "${file}\n")
endif()
```
