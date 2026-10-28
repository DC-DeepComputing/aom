---
name: libaom/arm-neon-use-mem-helpers-for-loads-and-stores
description: In ARM NEON routines and mem_neon.h, use standardized memory helpers, aggregate multi-row variants, and matching signedness in intrinsics, pointer casts, and loaders
rubric: readability
severity: should_fix
---

**Rule.** In ARM NEON SIMD implementations and helper headers (such as
mem_neon.h), use shared inline memory helper functions (such as load_u8_4x1,
store_u8_4x1, load_unaligned_u8_4x4, store_u8_8x2) to load, store, or transpose
vector memory slices instead of writing raw intrinsics (such as vld1_lane_*,
vst1_lane_*) with manual pointer casts. Prefer aggregate multi-row helpers (such
as load_unaligned_u8_4x4 or 4x8 variants) over chaining multiple smaller helper
calls with manual stride pointer arithmetic. At call sites, use matching
type-specific load helpers directly (e.g., load_s16_4x2 instead of loading into
uint16x4_t temporaries and calling vreinterpret_s16_u16). When defining new
memory helper functions and macros (e.g., store_u16_*, load_u16_*,
load_s16_4x11, store_u8_4x1_lane), follow the
{load|store}_[unaligned_]<type>_<width>x<height>[_lane] naming convention,
macro-parameterize immediate lane indices, and ensure all pointer casts,
intermediate scalar types, and SIMD intrinsics match the intended signedness
(e.g., uint32_t *, vst1_lane_u32, and vreinterpret_u32_u16 for unsigned
operations; do not mix signed intrinsics or casts in unsigned helpers, or vice
versa).

**Why.** Encapsulating multi-line vector memory operations in shared inline
helpers avoids error-prone pointer casts, manual lane index manipulation, and
repetitive register reinterpret operations (vreinterpret_*). Aggregate multi-row
helpers eliminate manual row-pointer offset arithmetic and reduce boilerplate
without runtime overhead. Standardized helper naming and macro parameterization
maintain a uniform SIMD memory API while satisfying immediate-lane compiler
constraints. Matching the signedness of pointer casts and SIMD intrinsics inside
helpers prevents -Wpointer-sign compiler warnings, avoids unnecessary casts at
call sites, and guarantees type safety and consistency across helper
definitions.

## Examples

Bad:

```cpp
// Anti-pattern 1: Raw intrinsics with pointer casts
vst1_lane_u32((uint32_t *)(src - 6 * stride), vreinterpret_u32_u8(p5q5), 0);
vst1_lane_u32((uint32_t *)(src - 5 * stride), vreinterpret_u32_u8(p4q4), 0);

// Anti-pattern 2: Chaining smaller helpers with manual stride offsets
load_unaligned_u8_4x2(src - 2, stride, (uint32x2_t *)&p1p0);
load_unaligned_u8_4x2((src - 2) + 2 * stride, stride, (uint32x2_t *)&q0q1);

// Anti-pattern 3: Loading unsigned temporaries and reinterpreting
uint16x4_t t0, t1;
int16x4_t s0, s1;
load_u16_4x2(s, src_stride, &t0, &t1);
s0 = vreinterpret_s16_u16(t0);
s1 = vreinterpret_s16_u16(t1);

// Anti-pattern 4: Mismatched signedness in helper definition
#define store_u16_2x1(s, s0, lane)                                 \
  do {                                                             \
    vst1_lane_s32((int32_t *)(s), vreinterpret_s32_u16(s0), lane); \
  } while (0)
```

Good:

```cpp
// Pattern 1: Shared memory helpers
store_u8_4x1(src - 6 * stride, p5q5, 0);
store_u8_4x1(src - 5 * stride, p4q4, 0);

// Pattern 2: Aggregate multi-row memory helper
load_unaligned_u8_4x4(src - 2, stride, &p1p0, &q0q1);

// Pattern 3: Direct type-specific load helper
int16x4_t s0, s1;
load_s16_4x2(s, src_stride, &s0, &s1);

// Pattern 4: Matching signedness in helper definition
#define store_u16_2x1(s, s0, lane)                                  \
  do {                                                              \
    vst1_lane_u32((uint32_t *)(s), vreinterpret_u32_u16(s0), lane); \
  } while (0)
```
