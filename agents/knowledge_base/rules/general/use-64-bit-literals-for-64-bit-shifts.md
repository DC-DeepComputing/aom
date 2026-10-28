---
name: libaom/use-64-bit-literals-for-64-bit-shifts
description: When shifting bitmasks or computing offsets in 64-bit integer operations, use explicit 64-bit literals like 1LL or (uint64_t)1 rather than plain 1
rubric: correctness
severity: should_fix
---

**Rule.** When shifting bitmasks or computing offsets for 64-bit integer
operations (such as int64_t or uint64_t), use explicit 64-bit integer literals
or casts (e.g., 1LL, 1ULL, (int64_t)1, or (uint64_t)1) instead of plain 1 or 1U.
Avoid unadorned 32-bit integer literals in shift expressions where the result or
shift amount exceeds 31 bits or feeds 64-bit arithmetic.

**Why.** In C and C++, an unadorned integer literal 1 has type int (typically
32-bit signed). Shifting 1 by 31 or more bits causes integer overflow and
undefined behavior, while shifting it in 64-bit expressions causes implicit
widening conversions and signedness issues. Using explicit 64-bit literals
ensures the shift operation and intermediate arithmetic evaluate in 64 bits.

## Examples

Bad:

```cpp
static INLINE int64_t div_shift_s64(int64_t x, int power) {
  return (x < 0 ? x + (1 << power) - 1 : x) >> power;
}

uint64_t mask = 1 << bit_index;  // Overflows if bit_index >= 31
```

Good:

```cpp
static INLINE int64_t div_shift_s64(int64_t x, int power) {
  return (x < 0 ? x + (1LL << power) - 1 : x) >> power;
}

uint64_t mask = 1ULL << bit_index;  // Evaluates safely in 64 bits
```
