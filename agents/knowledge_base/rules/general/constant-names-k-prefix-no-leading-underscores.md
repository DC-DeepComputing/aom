---
name: libaom/constant-names-k-prefix-no-leading-underscores
description: When declaring compile-time constants, static arrays, lookup tables, thresholds, or SIMD vector constants, use kCamelCase naming instead of snake_case, ALL_CAPS, or leading underscores
rubric: readability
severity: should_fix
---

**Rule.** When naming compile-time constants, local or file-scope constant
variables, fixed algorithmic thresholds, dimensions, static const lookup tables
(including DECLARE_ALIGNED array definitions), and SIMD vector constants, use
kCamelCase naming prefixed with 'k' (e.g., k0001, kMinTileArea, kLoadPad,
kSlidingWindowMask, kDotProdTranConcatTbl, kLeb128ByteMask, kColorThresh,
kBlockWidth, kVarThresh). Do not use leading underscores (e.g., _0001),
unprefixed snake_case (e.g., color_thresh, blk_w), or ALL_CAPS (e.g.,
MAX_BUFFER_SIZE). Even if legacy code contains older snake_case tables, use
kCamelCase for all newly added constants and reserve snake_case for
runtime-varying local variables and function parameters.

**Why.** Using the 'kCamelCase' naming convention clearly distinguishes
read-only compile-time constants, static lookup tables, and SIMD constants from
runtime-mutable variables, function parameters, and preprocessor macros
(ALL_CAPS). Avoiding leading underscores also prevents collisions with compiler-
or standard-library-reserved identifiers, maintaining clean and consistent
naming across codec modules.

## Examples

Bad:

```cpp
// Bad: snake_case, leading underscores, and ALL_CAPS for constants, SIMD vectors, and tables
const uint32x4_t _0001 = vdupq_n_u32(1);
const int color_thresh = 4;
const int blk_w = 16;
static const int MAX_BUFFER_SIZE = 1024;
static const uint8_t leb128_byte_mask = 0x7f;
DECLARE_ALIGNED(16, static const uint8_t, dot_prod_tran_concat_tbl[32]) = { ... };
```

Good:

```cpp
// Good: kCamelCase naming for all compile-time constants, SIMD vectors, and lookup tables
const uint32x4_t k0001 = vdupq_n_u32(1);
const int kColorThresh = 4;
const int kBlockWidth = 16;
static const int kMaxBufferSize = 1024;
static const uint8_t kLeb128ByteMask = 0x7f;
DECLARE_ALIGNED(16, static const uint8_t, kDotProdTranConcatTbl[32]) = { ... };
```
