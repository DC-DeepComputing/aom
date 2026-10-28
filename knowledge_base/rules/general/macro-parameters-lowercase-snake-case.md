---
name: libaom/macro-parameters-lowercase-snake-case
description: When defining function-like preprocessor macros, use lowercase snake_case names for macro parameters rather than uppercase identifiers or ALL_CAPS naming
rubric: readability
severity: nit
---

**Rule.** In preprocessor macro definitions, use lowercase snake_case names
(e.g., reg, feature, val) for macro parameters instead of uppercase identifiers
(REG, FEATURE). Reserve uppercase names for macro constants, enumerators, and
macro names.

**Why.** Uppercase names in C and C++ are reserved for macro constants,
enumerators, and macro names. Using lowercase or snake_case for macro parameters
aligns with standard function parameter conventions and avoids confusion or
accidental shadowing with macro and constant identifiers.

## Examples

Bad:

```cpp
#define FEATURE_SET(reg, FEATURE) (((reg) & (FEATURE##_BITS)) == (FEATURE##_BITS))
#define CLAMP(VAL, MIN_VAL, MAX_VAL) \
  (((VAL) < (MIN_VAL)) ? (MIN_VAL) : (((VAL) > (MAX_VAL)) ? (MAX_VAL) : (VAL)))
```

Good:

```cpp
#define FEATURE_SET(reg, feature) (((reg) & (feature##_BITS)) == (feature##_BITS))
#define CLAMP(val, min_val, max_val) \
  (((val) < (min_val)) ? (min_val) : (((val) > (max_val)) ? (max_val) : (val)))
```
