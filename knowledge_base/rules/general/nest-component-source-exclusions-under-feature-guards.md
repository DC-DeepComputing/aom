---
name: libaom/nest-component-source-exclusions-under-feature-guards
description: When filtering or modifying component-specific source lists in CMake scripts, nest exclusions and removals inside the component's feature guard block
rubric: design
severity: should_fix
---

**Rule.** In CMake build scripts, nest source-list exclusions, removals (such as
list(REMOVE_ITEM ...)), or modifications for component-specific targets inside
the corresponding component's feature guard block (e.g. if(CONFIG_AV1_ENCODER)
or if(CONFIG_AV1_DECODER)). Do not place sub-feature source exclusions at the
outer scope where target source variables may be unpopulated or inactive.

**Why.** Component-specific source variables (such as AOM_DSP_ENCODER_INTRIN_*
or AOM_AV1_ENCODER_SOURCES) are only defined and consumed when that component is
enabled. Scoping sub-feature exclusions and removals inside the component's
feature check keeps build logic cohesive, prevents unnecessary list operations
on unpopulated variables, and avoids configuration errors.

## Examples

Bad:

```cpp
if(CONFIG_AV1_ENCODER)
  ...
endif()

# Bad: Filtering encoder sources outside the CONFIG_AV1_ENCODER guard.
if(CONFIG_REALTIME_ONLY)
  list(REMOVE_ITEM AOM_DSP_ENCODER_INTRIN_AVX2
       "${AOM_ROOT}/aom_dsp/x86/variance_impl_avx2.c")
endif()
```

Good:

```cpp
if(CONFIG_AV1_ENCODER)
  ...
  # Good: Nested inside CONFIG_AV1_ENCODER where encoder sources are defined.
  if(CONFIG_REALTIME_ONLY)
    list(REMOVE_ITEM AOM_DSP_ENCODER_INTRIN_AVX2
         "${AOM_ROOT}/aom_dsp/x86/variance_impl_avx2.c")
  endif()
endif()
```
