---
name: libaom/conditionally-size-struct-bitfields-for-experiments
description: When expanding struct bitfields for experimental features or configurable limits, conditionally size bit widths with preprocessor guards instead of unconditionally increasing them
rubric: safety
severity: should_fix
---

**Rule.** When extending or declaring struct bitfields (such as mode info or
block parameters) to accommodate experimental features or larger configurable
limits (e.g., #if MAX_DRL_BITS > 3), conditionally compile the expanded bit
width with preprocessor checks. Do not unconditionally increase bitfield widths
for optional or experimental features in baseline data structures.

**Why.** Performance-critical structures like MB_MODE_INFO and block contexts
rely on tight bitfield packing to minimize cache footprint and memory
allocation. Unconditionally expanding bit widths for non-default or experimental
configurations wastes bits and risks enlarging object layouts across baseline
builds where the extra range is unused.

## Examples

Bad:

```cpp
// In av1/common/blockd.h (struct MB_MODE_INFO)
// Bad: Unconditionally expanded to 3 bits for experimental DRL support
uint8_t ref_mv_idx : 3;
```

Good:

```cpp
// In av1/common/blockd.h (struct MB_MODE_INFO)
// Good: Conditionally sized based on configuration limit
#if MAX_DRL_BITS > 3
uint8_t ref_mv_idx : 3;
#else
uint8_t ref_mv_idx : 2;
#endif
```
