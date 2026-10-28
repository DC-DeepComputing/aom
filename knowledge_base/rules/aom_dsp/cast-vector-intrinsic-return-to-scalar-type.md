---
name: libaom/aom_dsp/cast-vector-intrinsic-return-to-scalar-type
description: When adapting vector intrinsic outputs to satisfy scalar function parameters, cast the return value of the intrinsic call rather than its inner vector arguments
rubric: correctness
severity: should_fix
---

**Rule.** When passing vector intrinsic expressions into functions or macros
expecting scalar integer parameters, apply the scalar type cast to the return
value of the outer intrinsic call. Do not place the cast on inner vector
arguments passed into nested intrinsic operations.

**Why.** Placing a scalar type cast on an inner vector operand leaves the outer
intrinsic expression returning a vector type, resulting in compiler
type-mismatch errors or unintended type conversions when passed to scalar
parameters.

## Examples

Bad:

```cpp
// Incorrect: casting the inner vector argument instead of the outer intrinsic call
v128_from_64(0, vshr_n_u64((uint64_t)vreinterpret_u64_s64(vget_high_s64(a)), (n - 8) * 8));
```

Good:

```cpp
// Correct: casting the return value of the vector intrinsic to the expected scalar type
v128_from_64(0, (uint64_t)vshr_n_u64(vreinterpret_u64_s64(vget_high_s64(a)), (n - 8) * 8));
```
