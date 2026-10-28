---
name: libaom/aom_dsp/place-variable-identifier-before-dynamic-range-in-comments
description: When documenting intermediate variable properties in DSP or transform code, place the variable identifier and bit depth before the dynamic range
rubric: readability
severity: nit
---

**Rule.** When documenting intermediate variable properties (such as bit depth
and dynamic range bounds) in DSP and transform functions, state the variable
identifier and bit depth before the dynamic range. In single-line annotations,
use `<name>: <bit_depth> bit, dynamic range [<min>, <max>]`; in multi-line
comments across bit-depth configurations (such as 8b and HBD), place the
variable bit-depth line before the numerical dynamic range line.

**Why.** Placing the variable or buffer identifier and bit depth first
establishes the subject being described before stating its numerical bounds,
maintaining consistent comment ordering across DSP implementations and making
range derivations easier to read and verify.

## Examples

Bad:

```cpp
// Variant 1: Dynamic range placed before variable identifier and bit depth.
// dynamic range [-255, 255] (8b), [-4095, 4095] (HBD)
// src_diff: 9 bit (8b), 13 bit (HBD)

// Variant 2: Single-line range preceding variable identifier and bit depth.
// dynamic range [-16320, 16320], coeff: 15 bit
```

Good:

```cpp
// Variant 1: Variable and bit depth precede dynamic range across configurations.
// src_diff: 9 bit (8b), 13 bit (HBD)
// dynamic range [-255, 255] (8b), [-4095, 4095] (HBD)

// Variant 2: Standard single-line format with identifier and bit depth first.
// coeff: 15 bit, dynamic range [-16320, 16320]
```
