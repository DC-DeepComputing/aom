---
name: libaom/define-or-enum-for-c-simd-immediate-constants
description: 'When defining integer constants for immediate arguments to SIMD intrinsics in C, use #define or enum instead of static const'
rubric: correctness
severity: blocking
---

**Rule.** In C source and header files, define compile-time constants used as
immediate operands to SIMD intrinsics (such as ARM NEON shift/narrowing *_n_* or
lane *_lane_* intrinsics, x86 _mm_srli_* / _mm_alignr_*, and Wasm lane extract
intrinsics) using #define macros or enum enumerators instead of static const or
const variables. Ensure any expressions passed to immediate operand arguments
qualify as integer constant expressions (ICEs).

**Why.** In C99, static const int defines a read-only variable rather than an
integer constant expression (ICE). Compilers such as Clang and GCC strictly
enforce compile-time constant expressions for SIMD instruction immediate
arguments and fail compilation (e.g., 'argument to __builtin_neon_* must be a
constant integer') when const variables are passed.

## Examples

Bad:

```cpp
// In C headers:
static const int sm_weight_log2_scale = 8;

// Passing to NEON immediate shift/narrow intrinsic:
const uint16x4_t pred =
    vrshrn_n_u32(weighted_bl, sm_weight_log2_scale + 1);
```

Good:

```cpp
// Use #define or enum in C to produce an integer constant expression (ICE):
#define SMOOTH_WEIGHT_LOG2_SCALE 8
// or:
enum { sm_weight_log2_scale = 8 };

const uint16x4_t pred =
    vrshrn_n_u32(weighted_bl, SMOOTH_WEIGHT_LOG2_SCALE + 1);
```
