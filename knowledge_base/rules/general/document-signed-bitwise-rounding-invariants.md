---
name: libaom/document-signed-bitwise-rounding-invariants
description: When using bitwise right shifts or masks on signed integers for floor division or positive modulo, document the rounding invariants in an explanatory comment
rubric: readability
severity: nit
---

**Rule.** When relying on bitwise operations (such as arithmetic right shift
`>>` for floor division or bitwise AND `&` for non-negative modulo `[0, 2^k)`)
on signed integers instead of `/` and `%`, add an explanatory comment
documenting the rounding behavior. Detail the rounding invariants for negative
values and why standard arithmetic operators would not produce the desired
result.

**Why.** In C, integer division `/` and remainder `%` round toward zero,
producing negative remainders for negative dividends. Arithmetic right shift
`>>` rounds toward negative infinity and bitwise masking `& (2^k - 1)` always
yields a non-negative remainder in `[0, 2^k)`. Documenting this intentional
reliance on signed bitwise rounding prevents maintainers and reviewers from
mistaking the logic for a signedness bug.

## Examples

Bad:

```cpp
const int e1 = err >> b;
const int e2 = err & bmask;
return error_measure_lut[256 + e1] * (v - e2) +
       error_measure_lut[257 + e1] * e2;
```

Good:

```cpp
// Split error into two parts and do an interpolated table lookup.
// In C, >> and & round down and produce a positive remainder in [0, 2^b)
// for negative inputs, unlike / and % which round towards zero.
const int e1 = err >> b;
const int e2 = err & bmask;
return error_measure_lut[256 + e1] * (v - e2) +
       error_measure_lut[257 + e1] * e2;
```
