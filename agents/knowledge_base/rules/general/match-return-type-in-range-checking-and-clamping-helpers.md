---
name: libaom/match-return-type-in-range-checking-and-clamping-helpers
description: When implementing or modifying value range-checking, clamping, or pass-through helpers, match return types to input value types and provide dedicated 64-bit variants
rubric: correctness
severity: should_fix
---

**Rule.** Ensure value range-checking, clamping, and pass-through helper
functions return the exact same type and bit width as their input value
parameter. Define dedicated 64-bit variants (such as clamp64 or
range_check_value64) instead of accepting int64_t or uint64_t parameters and
truncating the return value to 32-bit types.

**Why.** When a range-checking or clamping function accepts a 64-bit value but
returns a 32-bit integer, values exceeding 32 bits are silently truncated at the
return site. This defeats 64-bit intermediate arithmetic in transform or
filtering pipelines and introduces silent integer overflow bugs.

## Examples

Bad:

```cpp
static inline int32_t range_check_value(int64_t value, int8_t bit) {
  const int64_t max_val = (1LL << (bit - 1)) - 1;
  const int64_t min_val = -(1LL << (bit - 1));
  assert(value <= max_val && value >= min_val);
  return (int32_t)value;
}
```

Good:

```cpp
static inline int32_t range_check_value(int32_t value, int8_t bit) {
  const int32_t max_val = (1 << (bit - 1)) - 1;
  const int32_t min_val = -(1 << (bit - 1));
  assert(value <= max_val && value >= min_val);
  return value;
}

static inline int64_t range_check_value64(int64_t value, int8_t bit) {
  const int64_t max_val = (1LL << (bit - 1)) - 1;
  const int64_t min_val = -(1LL << (bit - 1));
  assert(value <= max_val && value >= min_val);
  return value;
}
```
