---
name: libaom/convert-msvc-bitscan-index-to-clz
description: When implementing count leading zeros with MSVC _BitScanReverse or _BitScanReverse64 intrinsics, convert the MSB index via (total_bits - 1) ^ bit_index.
rubric: correctness
severity: blocking
---

**Rule.** Do not return the raw bit index from MSVC _BitScanReverse or
_BitScanReverse64 intrinsics when implementing count leading zeros (clz).
Convert the zero-based most-significant-bit index to a leading zero count using
(total_bits - 1) ^ bit_index (e.g., 31 ^ (int)first_set_bit for 32-bit or 63 ^
(int)first_set_bit for 64-bit).

**Why.** _BitScanReverse and _BitScanReverse64 return the zero-based index of
the most significant set bit (MSB, 0..31 or 0..63 where bit N-1 is highest).
Count leading zeros (CLZ) is the number of zero bits before the MSB, which is (N
- 1) - msb (or (N - 1) ^ msb). Returning the raw bit scan index directly
produces the MSB position rather than CLZ.

## Examples

Bad:

```cpp
static inline int aom_clzll(uint64_t n) {
  unsigned long first_set_bit;
  _BitScanReverse64(&first_set_bit, (unsigned __int64)n);
  return (int)first_set_bit;  // Returns MSB index (0..63), not leading zeros count!
}
```

Good:

```cpp
static inline int aom_clzll(uint64_t n) {
  assert(n != 0);
  unsigned long first_set_bit;
  _BitScanReverse64(&first_set_bit, (unsigned __int64)n);
  return 63 ^ (int)first_set_bit;  // or 63 - (int)first_set_bit
}
```
