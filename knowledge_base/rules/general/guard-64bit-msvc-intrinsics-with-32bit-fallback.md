---
name: libaom/guard-64bit-msvc-intrinsics-with-32bit-fallback
description: When using 64-bit MSVC intrinsics such as _BitScanReverse64, guard them with 64-bit architecture checks and provide a 32-bit fallback using 32-bit intrinsics
rubric: correctness
severity: blocking
---

**Rule.** When invoking 64-bit MSVC intrinsics (such as `_BitScanReverse64` or
`_BitScanForward64`) or declaring `#pragma intrinsic`, guard them with 64-bit
target checks (`#if defined(_M_X64) || defined(_M_ARM64)`). On 32-bit MSVC
targets (`_M_IX86`, `_M_ARM`), provide a portable fallback by splitting 64-bit
values into high and low 32-bit halves using 32-bit intrinsics (such as
`_BitScanReverse`).

**Why.** 64-bit MSVC intrinsics like `_BitScanReverse64` are only defined for
64-bit compilation targets (`_M_X64`, `_M_ARM64`) and cause compilation errors
on 32-bit MSVC targets (`_M_IX86`, `_M_ARM`). Providing a 32-bit fallback
ensures cross-platform compatibility across all supported Windows architectures.

## Examples

Bad:

```cpp
#pragma intrinsic(_BitScanReverse64)

static INLINE int aom_clzll(uint64_t n) {
  unsigned long first_set_bit;
  _BitScanReverse64(&first_set_bit, n);
  return 63 ^ (int)first_set_bit;
}
```

Good:

```cpp
#if defined(_M_X64) || defined(_M_ARM64)
#pragma intrinsic(_BitScanReverse64)
#endif

static INLINE int aom_clzll(uint64_t n) {
  assert(n != 0);
  unsigned long first_set_bit;
#if defined(_M_X64) || defined(_M_ARM64)
  _BitScanReverse64(&first_set_bit, (unsigned __int64)n);
  return 63 ^ (int)first_set_bit;
#else
  const unsigned long n_hi = (unsigned long)(n >> 32);
  if (n_hi != 0) {
    _BitScanReverse(&first_set_bit, n_hi);
    return 31 ^ (int)first_set_bit;
  }
  _BitScanReverse(&first_set_bit, (unsigned long)n);
  return 63 ^ (int)first_set_bit;
#endif
}
```
