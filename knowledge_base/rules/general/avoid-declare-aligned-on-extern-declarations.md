---
name: libaom/avoid-declare-aligned-on-extern-declarations
description: When declaring external variables with extern in header files, do not wrap declarations in DECLARE_ALIGNED; specify alignment only on definitions
rubric: readability
severity: nit
---

**Rule.** Do not use DECLARE_ALIGNED on extern variable declarations in header
files. Specify alignment constraints only on variable definitions where memory
storage is allocated.

**Why.** Alignment macros such as DECLARE_ALIGNED specify storage allocation
constraints for variable definitions. Using them on extern declarations in
header files is redundant, adds visual noise, and obscures the declaration
syntax.

## Examples

Bad:

```cpp
DECLARE_ALIGNED(8, extern const int8_t,
                av1_warped_filter[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8]);
```

Good:

```cpp
extern const WarpedFilterCoeff
    av1_warped_filter[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8];
```
