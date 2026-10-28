---
name: libaom/update-bit-width-guards-when-widening-signed-shifts-to-64-bi
description: When widening signed bit-range computations or clamping to 64-bit integers, update bit-width guards from 31 to 63 to prevent range truncation and shift undefined behavior
rubric: correctness
severity: should_fix
---

**Rule.** When widening signed bit-range computations, dynamic shifts, or
range-checking and clamping functions from 32-bit to 64-bit integers (e.g. using
1LL << (bit - 1) or (int64_t)1 << (bit - 1)), update the maximum allowable
bit-width guard (such as AOMMIN(bit, 31)) to 63. Do not retain 32-bit width caps
(31) when performing 64-bit arithmetic or bounds calculations.

**Why.** In C, left-shifting a signed integer to the sign bit (bit - 1 == 31 for
int32_t or bit - 1 == 63 for int64_t) or beyond causes signed integer overflow
and undefined behavior. Retaining a 31-bit upper bound when widening to 64-bit
arithmetic prematurely clamps allowable bit depths to 31-bit ranges, while
failing to guard against bit > 63 causes undefined behavior on 64-bit shifts.

## Examples

Bad:

```cpp
// Retaining 31 when widening to 64-bit clamp truncates range:
bit = AOMMIN(bit, 31);
return clamp64(value, -(1LL << (bit - 1)), (1LL << (bit - 1)) - 1);
```

Good:

```cpp
// Updating bit upper bound to 63 allows full 64-bit range without shift UB:
bit = AOMMIN(bit, 63);
return clamp64(value, -(1LL << (bit - 1)), (1LL << (bit - 1)) - 1);
```
