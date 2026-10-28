---
name: libaom/cmakelists-txt/omit-redundant-dot-slash-in-cmake-install-interface
description: When specifying relative include paths in CMake $<INSTALL_INTERFACE:...> generator expressions, omit redundant leading ./ prefixes and write bare relative paths
rubric: readability
severity: nit
---

**Rule.** In CMake $<INSTALL_INTERFACE:...> generator expressions, specify
relative include paths without leading ./ prefixes (e.g., use
$<INSTALL_INTERFACE:include> rather than $<INSTALL_INTERFACE:./include>).

**Why.** CMake automatically evaluates relative paths inside
$<INSTALL_INTERFACE:...> generator expressions relative to CMAKE_INSTALL_PREFIX.
A leading ./ prefix is redundant and non-idiomatic in CMake build
configurations.

## Examples

Bad:

```cpp
target_include_directories(aom PUBLIC $<INSTALL_INTERFACE:./include>)
```

Good:

```cpp
target_include_directories(aom PUBLIC $<INSTALL_INTERFACE:include>)
```
