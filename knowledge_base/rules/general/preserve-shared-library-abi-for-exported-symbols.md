---
name: libaom/preserve-shared-library-abi-for-exported-symbols
description: When modifying exported function signatures or shared library export lists, preserve ABI compatibility in minor releases and avoid exporting internal test symbols
rubric: correctness
severity: nit
---

**Rule.** Do not change function signatures, parameter lists, or return types of
symbols listed in shared library export files (such as `aom/exports_enc` or
`aom/exports_dec`) during minor releases. Defer ABI-breaking signature updates
to major version releases, documenting the planned change with an attributed
`TODO(crbug.com/...):` comment. Additionally, do not add internal symbols to
export lists merely to satisfy unit test linking in shared library builds; guard
internal unit tests with `if(NOT BUILD_SHARED_LIBS)` in CMake instead.

**Why.** Exported symbols constitute the public binary interface (ABI) of the
shared object library. Altering signatures in minor releases breaks binary
compatibility for downstream applications linking against libaom. Exporting
internal symbols just for test suites pollutes the public shared library ABI and
leaks implementation details.

## Examples

Bad:

```cpp
// In a minor release, changing the signature or return type of an exported API:
int aom_film_grain_table_append(aom_film_grain_table_t *t, int64_t time_stamp,
                                int64_t end_time,
                                const aom_film_grain_t *grain);

// In aom/exports_enc, adding an internal function to fix shared library test linking:
text av1_internal_helper_for_test
```

Good:

```cpp
// In a minor release, defer ABI-breaking changes to a major release with a TODO:
// TODO(https://crbug.com/aomedia/3228): Update this function to return an
// integer status.
void aom_film_grain_table_append(aom_film_grain_table_t *t, int64_t time_stamp,
                                 int64_t end_time,
                                 const aom_film_grain_t *grain);

// In test/test.cmake, exclude internal unit tests from shared library builds:
if(NOT BUILD_SHARED_LIBS)
  list(APPEND AOM_UNIT_TEST_COMMON_SOURCES "${AOM_ROOT}/test/internal_test.cc")
endif()
```
