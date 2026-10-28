---
name: libaom/avoid-misleading-arch-suffixes-on-semantically-distinct-help
description: When defining architecture-specific helpers with suffixes like _neon or _ssse3, do not reuse base function names if the helper checks different conditions or narrower semantics
rubric: readability
severity: should_fix
---

**Rule.** Do not reuse the same base function name with an architecture suffix
(such as `_neon`, `_ssse3`, `_avx2`, or `_sve`) for internal helper functions
that evaluate different conditions, narrower constraints, or distinct algorithm
paths compared to the generic function. Name architecture-specific helpers after
their concrete purpose (such as `has_normative_scaler_neon` rather than
`av1_has_optimized_scaler_neon`) so the suffix only denotes hardware-accelerated
variants of identical logic.

**Why.** Appending an architecture suffix implies that the function is a direct
hardware-accelerated implementation of the base function with identical
semantics. If a helper only tests for a specialized subset of conditions (such
as normative scaling ratios rather than overall optimized scaler availability),
reusing the base name misleads maintainers about the function's scope and
responsibilities.

## Examples

Bad:

```cpp
// av1/common/resize.h:
bool av1_has_optimized_scaler(int src_w, int src_h, int dst_w, int dst_h);

// av1/common/arm/resize_neon.c:
// Misleading: helper only checks normative ratios (1/2, 1/4, 3/4), not full scaler availability
static INLINE bool av1_has_optimized_scaler_neon(int src_w, int src_h,
                                                 int dst_w, int dst_h) {
  return (2 * dst_w == src_w && 2 * dst_h == src_h) ||
         (4 * dst_w == src_w && 4 * dst_h == src_h);
}
```

Good:

```cpp
// av1/common/resize.h:
bool av1_has_optimized_scaler(int src_w, int src_h, int dst_w, int dst_h);

// av1/common/arm/resize_neon.c:
// Clear: function name accurately reflects that it tests specifically for normative scaling ratios
static INLINE bool has_normative_scaler_neon(int src_w, int src_h,
                                             int dst_w, int dst_h) {
  return (2 * dst_w == src_w && 2 * dst_h == src_h) ||
         (4 * dst_w == src_w && 4 * dst_h == src_h);
}
```
