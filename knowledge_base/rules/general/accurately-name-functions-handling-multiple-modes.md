---
name: libaom/accurately-name-functions-handling-multiple-modes
description: When naming functions or helper routines that process multiple features or modes, include all handled modes in the identifier rather than omitting supported variants
rubric: readability
severity: nit
---

**Rule.** When naming functions or helper routines that process, compute, or
dispatch multiple distinct modes, tools, or feature categories, ensure the
identifier accurately reflects all supported modes rather than naming only a
subset.

**Why.** Naming a function after only a subset of the modes or features it
handles misleads readers into believing unsupported modes are handled elsewhere,
causing maintenance confusion and redundant duplicate helpers.

## Examples

Bad:

```cpp
// Misleading: function handles both directional intra and filter intra modes
static void build_directional_intra_predictors(
    const uint8_t *ref, int ref_stride, uint8_t *dst, int dst_stride,
    PREDICTION_MODE mode, int p_angle, FILTER_INTRA_MODE filter_intra_mode, ...);
```

Good:

```cpp
// Accurately names both supported intra prediction modes:
static void build_directional_and_filter_intra_predictors(
    const uint8_t *ref, int ref_stride, uint8_t *dst, int dst_stride,
    PREDICTION_MODE mode, int p_angle, FILTER_INTRA_MODE filter_intra_mode, ...);
```
