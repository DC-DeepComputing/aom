---
name: libaom/preserve-cxx14-baseline-avoid-cxx17
description: When writing C++ code or configuring CMake targets, adhere to libaom's baseline C++14 standard and avoid C++17 features like std::optional or CXX_STANDARD 17
rubric: readability
severity: should_fix
---

**Rule.** Do not use C++17 or newer language/library features (such as
std::optional, std::variant, std::string_view, or if constexpr) and do not set
CXX_STANDARD 17 in CMake targets. Maintain compatibility with libaom's baseline
C++14 standard across all source files, examples, and tools.

**Why.** libaom follows Google Open Source Support Policies requiring baseline
C++ compatibility (C++14 or earlier) to support older toolchains and default OS
distributions (such as CMake 3.22.1 on Ubuntu 22.04). Using C++17 features or
setting CXX_STANDARD 17 breaks builds on those environments.

## Examples

Bad:

```cpp
// In CMakeLists.txt:
set_property(TARGET svc_encoder_rtc PROPERTY CXX_STANDARD 17)

// In source or header files:
#include <optional>
std::optional<ColorProperties> alpha_color_description;
```

Good:

```cpp
// In CMakeLists.txt:
# Retain baseline C++14 standard for target

// In source or header files:
std::pair<ColorProperties, bool> alpha_color_description;
```
