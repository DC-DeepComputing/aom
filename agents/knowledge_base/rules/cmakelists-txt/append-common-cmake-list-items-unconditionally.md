---
name: libaom/cmakelists-txt/append-common-cmake-list-items-unconditionally
description: When appending source files or targets to CMake lists across conditional branches, append common items unconditionally and use conditionals only for variant-specific items
rubric: design
severity: nit
---

**Rule.** In CMake build scripts, append common target or source items to lists
unconditionally, and use `if()` conditions only to append configuration-specific
items rather than duplicating shared elements across conditional branches.

**Why.** Duplicating shared targets or source files across multiple conditional
branches (such as `if(CONFIG_*)` and `if(NOT CONFIG_*)`) increases maintenance
overhead and creates divergence bugs when targets change. Factoring common
entries into an unconditional append keeps build lists concise and isolated to
specific configuration differences.

## Examples

Bad:

```cpp
if(NOT CONFIG_REALTIME_ONLY)
  list(APPEND AOM_ENCODER_EXAMPLE_TARGETS aomenc lossless_encoder
              noise_model photon_noise_table set_maps simple_encoder
              scalable_encoder svc_encoder_rtc twopass_encoder)
endif()
if(CONFIG_REALTIME_ONLY)
  list(APPEND AOM_ENCODER_EXAMPLE_TARGETS aomenc lossless_encoder set_maps
              simple_encoder scalable_encoder svc_encoder_rtc
              twopass_encoder)
endif()
```

Good:

```cpp
list(APPEND AOM_ENCODER_EXAMPLE_TARGETS aomenc lossless_encoder set_maps
            simple_encoder scalable_encoder svc_encoder_rtc twopass_encoder)
if(NOT CONFIG_REALTIME_ONLY)
  list(APPEND AOM_ENCODER_EXAMPLE_TARGETS noise_model photon_noise_table)
endif()
```
