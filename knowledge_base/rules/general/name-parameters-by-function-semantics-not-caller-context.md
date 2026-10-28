---
name: libaom/name-parameters-by-function-semantics-not-caller-context
description: When naming function parameters and configuration flags, describe what the parameter controls within the callee rather than a caller-specific feature or condition
rubric: readability
severity: nit
---

**Rule.** When declaring or naming function parameters, name each parameter
according to what it configures, represents, or passes to downstream callees
within the function (e.g., alloc_y_buffer_8bit). Do not name parameters after a
specific caller's feature, subsystem, or gating condition (e.g.,
enable_global_motion).

**Why.** Naming parameters after a specific caller's context leaks external
feature assumptions into reusable helper functions, obscures what the parameter
actually controls or forwards, and creates confusion when other callers invoke
the function for different purposes.

## Examples

Bad:

```cpp
// Leaking caller feature context into a generic scaling helper:
YV12_BUFFER_CONFIG *av1_realloc_and_scale_if_required(
    AV1_COMMON *cm, YV12_BUFFER_CONFIG *unscaled, YV12_BUFFER_CONFIG *scaled,
    const InterpFilter filter, const int phase, const bool use_optimized_scaler,
    const bool for_psnr, const int border_in_pixels,
    const bool enable_global_motion);
```

Good:

```cpp
// Naming parameter based on what the function/callee actually configures:
YV12_BUFFER_CONFIG *av1_realloc_and_scale_if_required(
    AV1_COMMON *cm, YV12_BUFFER_CONFIG *unscaled, YV12_BUFFER_CONFIG *scaled,
    const InterpFilter filter, const int phase, const bool use_optimized_scaler,
    const bool for_psnr, const int border_in_pixels,
    const bool alloc_y_buffer_8bit);
```
