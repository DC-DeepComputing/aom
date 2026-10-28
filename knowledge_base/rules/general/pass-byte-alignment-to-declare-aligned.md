---
name: libaom/pass-byte-alignment-to-declare-aligned
description: When using `DECLARE_ALIGNED`, pass byte alignment rather than bit widths or dimension constants, and omit alignment unless required by wider-type or aligned SIMD access
rubric: correctness
severity: should_fix
---

**Rule.** When declaring aligned variables or arrays with `DECLARE_ALIGNED(n,
typ, val)`, supply the required alignment boundary `n` strictly in bytes (e.g.,
4, 16, 32, or 64) as the first argument, and specify array dimensions inside the
variable declaration `val`. Do not pass bit widths (e.g., passing 32 or 128
instead of 4 or 16 bytes), and do not pass buffer lengths, block sizes, or
dimension constants (e.g., `SUBBLOCK_SIZE`). Additionally, omit
`DECLARE_ALIGNED` on 1-byte arrays, scalar byte types, or static/const arrays
unless elements are actually accessed via wider integer types (such as
`uint16_t*` or `uint32_t*`) or aligned SIMD instructions; leave arrays unaligned
when elements are only loaded via byte pointers or unaligned load intrinsics
(such as ARM NEON `vld1_u8`).

**Why.** `DECLARE_ALIGNED` expands to compiler alignment attributes (such as
`__attribute__((aligned(n)))` and `__declspec(align(n))`), where `n` represents
byte count. Passing bit widths causes excessive over-alignment and padding,
while passing buffer dimensions or block sizes risks under-alignment or
misaligned SIMD access (risking bus errors, faults, or performance regressions).
Conversely, applying `DECLARE_ALIGNED` to 1-byte arrays or arrays accessed
solely as bytes or via unaligned load intrinsics imposes redundant alignment
requirements and clutters the code without functional benefit.

## Examples

Bad:

```cpp
// Bad: Passing bit width (32 bits) instead of bytes (4 bytes).
DECLARE_ALIGNED(32, static const uint8_t, obmc_mask_4[4]) = { 39, 50, 59, 64 };

// Bad: Redundant alignment directive on a 1-byte array.
DECLARE_ALIGNED(32, static const uint8_t, obmc_mask_1[1]) = { 64 };

// Bad: Passing block dimension constant as alignment argument.
DECLARE_ALIGNED(SUBBLOCK_SIZE, static const uint8_t,
                av1_all_zeros[MAX_SB_SIZE]) = { 0 };

// Bad: Unnecessary alignment on an 8-byte array only read via unaligned byte loads (vld1_u8).
DECLARE_ALIGNED(4, static const uint8_t, obmc_mask_8[8]) = {
  36, 42, 48, 53, 57, 61, 64, 64
};
```

Good:

```cpp
// Good: Specify byte alignment (4 bytes) when accessed via wider types (uint32_t*).
DECLARE_ALIGNED(4, static const uint8_t, obmc_mask_4[4]) = { 39, 50, 59, 64 };

// Good: 1-byte arrays are inherently aligned; omit DECLARE_ALIGNED.
static const uint8_t obmc_mask_1[1] = { 64 };

// Good: Pass explicit SIMD byte alignment (16 or 32 bytes) as first argument.
DECLARE_ALIGNED(16, static const uint8_t,
                av1_all_zeros[SUBBLOCK_SIZE]) = { 0 };

// Good: Leave byte-accessed or unaligned SIMD (vld1_u8) loaded arrays unaligned.
static const uint8_t obmc_mask_8[8] = { 36, 42, 48, 53, 57, 61, 64, 64 };
```
