---
name: libaom/name-bitdepth-dispatchers-base-and-qualify-variants
description: When defining bit-depth dispatching functions, use the unqualified base name for the dispatcher and qualify implementations with lowbd and highbd
rubric: readability
severity: nit
---

**Rule.** When implementing bit-depth dispatching functions, use the unqualified
base name for the dispatcher (e.g., `aom_calc_ssim`) and qualify the specific
implementations symmetrically with `lowbd` and `highbd` (e.g.,
`aom_lowbd_calc_ssim` and `aom_highbd_calc_ssim`). Do not give the unqualified
base name solely to the low-bitdepth implementation or append suffixes like
`_facade` to the dispatcher.

**Why.** Standardizing the unqualified name on the bit-depth dispatcher provides
a clean, predictable API for callers while keeping low-bitdepth and
high-bitdepth variants consistently named across libaom.

## Examples

Bad:

```cpp
// Anti-pattern: unqualified name handles only lowbd, or dispatcher uses _facade
void aom_calc_ssim(const YV12_BUFFER_CONFIG *source, ...);  // 8-bit only
void aom_highbd_calc_ssim(const YV12_BUFFER_CONFIG *source, ...);
void aom_calc_ssim_facade(const YV12_BUFFER_CONFIG *orig, ..., int is_hbd, ...);
```

Good:

```cpp
// Idiomatic: dispatcher uses base name; implementations are qualified
void aom_lowbd_calc_ssim(const YV12_BUFFER_CONFIG *source, ...);
void aom_highbd_calc_ssim(const YV12_BUFFER_CONFIG *source, ...);
void aom_calc_ssim(const YV12_BUFFER_CONFIG *orig, ..., int is_hbd, ...);
```
