---
name: libaom/consolidate-cmake-conditional-source-lists
description: When adding or modifying conditional source lists in CMake files, consolidate appends for the same configuration flag into a single if block
rubric: design
severity: nit
---

**Rule.** In CMake build files, consolidate source list additions (`list(APPEND
...)` or `target_sources(...)`) for a feature, platform, or configuration flag
into a single `if(...)` block rather than scattering multiple separate `if(...)`
checks for the same condition across the file.

**Why.** Scattering repetitive `if(CONFIG_...)` conditional checks across build
scripts increases verbosity, fragments feature-specific source definitions, and
makes build configurations harder to navigate and maintain.

## Examples

Bad:

```cpp
list(APPEND AOM_DSP_COMMON_INTRIN_SSE2
            "${AOM_ROOT}/aom_dsp/x86/loopfilter_sse2.c")
if(CONFIG_AV1_HIGHBITDEPTH)
  list(APPEND AOM_DSP_COMMON_INTRIN_SSE2
              "${AOM_ROOT}/aom_dsp/x86/highbd_loopfilter_sse2.c")
endif()

list(APPEND AOM_DSP_COMMON_INTRIN_SSSE3
            "${AOM_ROOT}/aom_dsp/x86/intrapred_ssse3.c")
if(CONFIG_AV1_HIGHBITDEPTH)
  list(APPEND AOM_DSP_COMMON_INTRIN_SSSE3
              "${AOM_ROOT}/aom_dsp/x86/highbd_convolve_ssse3.c")
endif()
```

Good:

```cpp
list(APPEND AOM_DSP_COMMON_INTRIN_SSE2
            "${AOM_ROOT}/aom_dsp/x86/loopfilter_sse2.c")
list(APPEND AOM_DSP_COMMON_INTRIN_SSSE3
            "${AOM_ROOT}/aom_dsp/x86/intrapred_ssse3.c")

if(CONFIG_AV1_HIGHBITDEPTH)
  list(APPEND AOM_DSP_COMMON_INTRIN_SSE2
              "${AOM_ROOT}/aom_dsp/x86/highbd_loopfilter_sse2.c")
  list(APPEND AOM_DSP_COMMON_INTRIN_SSSE3
              "${AOM_ROOT}/aom_dsp/x86/highbd_convolve_ssse3.c")
endif()
```
