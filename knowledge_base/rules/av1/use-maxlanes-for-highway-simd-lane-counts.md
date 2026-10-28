---
name: libaom/av1/use-maxlanes-for-highway-simd-lane-counts
description: When implementing Highway SIMD routines, size lane-dependent data structures and tail masking logic using hn::MaxLanes(tag) or hn::Lanes(tag) instead of hardcoding fixed lane counts
rubric: correctness
severity: should_fix
---

**Rule.** In Highway SIMD code, size lane-dependent data structures (such as
mask lookup arrays or temporary lane buffers) and tail masking or loop step
logic using `hn::MaxLanes(tag)` or `hn::Lanes(tag)` rather than hardcoding fixed
vector lane counts. Ensure loop bounds and mask indexing dynamically adapt to
the target vector width.

**Why.** Highway targets multiple SIMD instruction sets with varying vector
widths (e.g., 4 lanes for SSE4/NEON, 8 lanes for AVX2, and 16 lanes for AVX-512
int32). Hardcoding fixed lane counts causes buffer overflows on wider
architectures and incorrect tail masking or logic bugs across different target
SIMD instruction sets.

## Examples

Bad:

```cpp
HWY_ALIGN hn::Mask<decltype(int32_tag)> mask[8];
for (int idx = 0; idx < 8; idx++) {
  mask[idx] = hn::FirstN(int32_tag, idx);
}
int idx = AOMMIN(8, width + 1 - j);
if (idx < 8) {
  sum1 = hn::IfThenElseZero(mask[idx], sum1);
}
```

Good:

```cpp
const int max_lanes = static_cast<int>(hn::MaxLanes(int32_tag));
HWY_ALIGN hn::Mask<decltype(int32_tag)> mask[max_lanes];
for (int idx = 0; idx < max_lanes; idx++) {
  mask[idx] = hn::FirstN(int32_tag, idx);
}
int idx = AOMMIN(max_lanes, width + 1 - j);
if (idx < max_lanes) {
  sum1 = hn::IfThenElseZero(mask[idx], sum1);
}
```
