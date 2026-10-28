---
name: libaom/compute-bit-depth-max-value-with-shift
description: When computing the maximum pixel value or mask for a given bit depth, use (1 << bd) - 1 instead of chained ternary expressions
rubric: readability
severity: should_fix
---

**Rule.** Use `(1 << bd) - 1` or `(1 << bit_depth) - 1` to compute the maximum
pixel value or bitmask for a given bit depth, rather than writing chained
ternary expressions or hardcoded conditional branches for specific bit depths.

**Why.** Computing `(1 << bd) - 1` is standard across the codebase, directly
calculates the maximum value for any bit depth without branching, and avoids
verbose ternary ladders with hardcoded constants.

## Examples

Bad:

```cpp
// Chained ternary expression hardcoding specific bit depths
const uint16x8_t max =
    vdupq_n_u16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
```

Good:

```cpp
// Direct arithmetic expression valid for all bit depths
const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
```
