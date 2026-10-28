---
name: libaom/store-directly-from-neon-vectors-using-lane-indices
description: When storing sub-vector elements from 128-bit ARM NEON vectors, use 128-bit lane-store intrinsics or helpers with direct lane indices instead of vget_high
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON SIMD routines, when storing sub-vector elements or
multi-sample chunks (such as 2x16-bit, 4x8-bit, or 32-bit elements) from a
128-bit vector, use 128-bit lane-store intrinsics or helpers (such as
vst1q_lane_* or store_*q_*) with direct lane indices. Do not split the 128-bit
vector using vget_high_* to pass 64-bit vectors to 64-bit store intrinsics.

**Why.** Extracting the upper 64 bits of a 128-bit NEON vector with vget_high_*
can force compilers to emit an unnecessary register MOV or DUP instruction.
Storing directly from the 128-bit vector using lane-indexed store operations
accesses higher lanes directly in the store instruction, eliminating redundant
move instructions and reducing register pressure.

## Examples

Bad:

```cpp
// Splitting a 128-bit vector with vget_high generates redundant MOV instructions
store_u16_2x1(d + 0 * dst_stride, vget_low_u16(d01), 0);
store_u16_2x1(d + 1 * dst_stride, vget_high_u16(d01), 0);
```

Good:

```cpp
// Store directly from the 128-bit vector using direct lane indices
store_u16q_2x1(d + 0 * dst_stride, d01, 0);
store_u16q_2x1(d + 1 * dst_stride, d01, 1);
```
