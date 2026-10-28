---
name: libaom/parameterize-neon-lane-helpers-via-macros
description: When wrapping NEON intrinsics requiring compile-time constant lane indices, use parameterized preprocessor macros instead of duplicating separate per-lane functions
rubric: design
severity: should_fix
---

**Rule.** When implementing helper routines around ARM NEON intrinsics that
require compile-time constant lane indices (such as vget_lane_*, vset_lane_*,
vdup_lane_*, vld1_lane_*, or vst1_lane_*), define a parameterized preprocessor
macro accepting a lane argument. Do not duplicate separate static inline
functions for each individual lane index (such as _lane0, _lane1).

**Why.** ARM NEON lane intrinsics require immediate constant lane values at
compile time, preventing a single C static inline function with a runtime lane
parameter. Using a parameterized macro avoids duplicating identical boilerplate
functions across each lane index while satisfying the compiler's immediate-value
constraint.

## Examples

Bad:

```cpp
static INLINE void store_u8_4x1_lane0(uint8_t *dst, const uint8x8_t src) {
  uint32_t a = vget_lane_u32(vreinterpret_u32_u8(src), 0);
  memcpy(dst, &a, 4);
}
static INLINE void store_u8_4x1_lane1(uint8_t *dst, const uint8x8_t src) {
  uint32_t a = vget_lane_u32(vreinterpret_u32_u8(src), 1);
  memcpy(dst, &a, 4);
}
```

Good:

```cpp
#define store_u8_4x1_lane(dst, src, lane)              \
  do {                                                 \
    uint32_t a =                                       \
        vget_lane_u32(vreinterpret_u32_u8(src), lane); \
    memcpy((dst), &a, 4);                              \
  } while (0)
```
