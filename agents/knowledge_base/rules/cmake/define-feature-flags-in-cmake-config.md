---
name: libaom/cmake/define-feature-flags-in-cmake-config
description: 'When adding feature or experiment toggles, avoid hardcoding header #define macros; use set_aom_config_var in cmake/aom_config_defaults.cmake for build flags or struct fields/local variables for runtime toggles.'
rubric: readability
severity: should_fix
---

**Rule.** Do not define experiment switches, feature toggles, or temporary test
flags as hardcoded `#define` macros in header or source files.

- **Build-time / global experiment flags**: Define them as CMake configuration variables (`CONFIG_<FEATURE>`) in `cmake/aom_config_defaults.cmake` using `set_aom_config_var(CONFIG_<FEATURE> <default_val> "<description>")`. In C/C++ source code, guard conditional implementations with `#if CONFIG_<FEATURE>`.
- **Runtime / local toggles**: If a switch is function- or file-local, declare it as a local variable or static constant in the C source file. If it must be configurable across modules or dynamically tuned at runtime, store it in encoder/decoder context structs (such as `cpi->rc` or `cpi->oxcf`) or expose it via codec controls.

**Why.** Hardcoding feature toggles and flags as global `#define` macros in
headers pollutes the preprocessor namespace and prevents options from being
configured at build time (e.g., via `-DCONFIG_<FEATURE>=0/1` across CMake and CI
matrices) or dynamically adjusted per encoder/decoder instance at runtime.
Centralizing build options in `cmake/aom_config_defaults.cmake` ensures
consistent config header generation across translation units, while using
context structs and local variables preserves runtime flexibility.

## Examples

Bad:

```cpp
// Variant 1: Hardcoded build-time experiment toggle in header
// av1/common/enums.h
#define ADAPT_FILTER_INTRA 0

#if ADAPT_FILTER_INTRA
typedef enum {
  ADAPT_FILTER_INTRA_NONE,
  ADAPT_FILTER_INTRA_MODE_1,
} ADAPT_FILTER_INTRA_MODE;
#endif

// Variant 2: Global header macro for local/runtime algorithm toggle
// av1/encoder/ratectrl.h
#define ONEHALFONLY_RESIZE 1
```

Good:

```cpp
// Variant 1: CMake configuration option for build-time experiment
// cmake/aom_config_defaults.cmake
set_aom_config_var(CONFIG_ADAPT_FILTER_INTRA 0 "AV1 experiment flag.")

// av1/common/enums.h
#if CONFIG_ADAPT_FILTER_INTRA
typedef enum {
  ADAPT_FILTER_INTRA_NONE,
  ADAPT_FILTER_INTRA_MODE_1,
} ADAPT_FILTER_INTRA_MODE;
#endif

// Variant 2: Local variable or context struct field for runtime toggle
// av1/encoder/ratectrl.c
static void dynamic_resize_one_pass_cbr(AV1_COMP *cpi) {
  const int one_half_only = 1;  // or use cpi->oxcf / cpi->rc struct field
  resize_action = one_half_only ? DOWN_ONEHALF : DOWN_THREEFOUR;
}
```
