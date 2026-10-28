---
name: libaom/cmakelists-txt/cmake-project-version-matches-release
description: When specifying the library version in CMake project declarations, ensure the VERSION argument matches the current official library release version
rubric: correctness
severity: should_fix
---

**Rule.** When defining or updating the project version in CMake via project(...
VERSION <version>), ensure the version matches the current official release
version of the library rather than an unreleased or arbitrary version.

**Why.** CMake's PROJECT_VERSION is used during package export and package
version configuration (such as write_basic_package_version_file). Mismatched
version numbers export incorrect metadata to package managers and downstream
consumers.

## Examples

Bad:

```cpp
# Current official release is 3.12.0
project(AOM LANGUAGES C CXX VERSION 3.12.1)
```

Good:

```cpp
# Matches current official release
project(AOM LANGUAGES C CXX VERSION 3.12.0)
```
