---
name: libaom/group-correlated-lookup-properties-into-struct-arrays
description: When defining lookup tables for correlated properties (such as numerator/denominator pairs), group them into a single struct array rather than maintaining separate parallel arrays
rubric: design
severity: should_fix
---

**Rule.** Group correlated lookup table properties (such as numerator and
denominator pairs, width/height dimensions, or min/max bounds) into a single
struct and use an array of structs rather than maintaining separate parallel
arrays. Define and share the struct type across subsystems when the table values
or attributes are used by both encoder and decoder modules.

**Why.** Maintaining separate parallel arrays scatters related data, increases
the risk of out-of-sync array indexing or mismatch bugs during maintenance, and
forces function signatures to accept multiple separate array arguments.
Packaging coupled properties into a struct ensures consistency, simplifies
access, and enables passing the coupled attributes as a single cohesive unit.

## Examples

Bad:

```cpp
// Anti-pattern: Maintaining separate parallel arrays for coupled properties
static const int superres_scale_numerators[SUPERRES_SCALES] = { 8, 8, 8, 8 };
static const int superres_scale_denominators[SUPERRES_SCALES] = { 10, 12, 14, 16 };

int num = superres_scale_numerators[idx];
int denom = superres_scale_denominators[idx];
```

Good:

```cpp
// Group correlated attributes into a single struct and array of structs
typedef struct {
  uint8_t scale_num;
  uint8_t scale_denom;
} ScaleFactor;

static const ScaleFactor superres_scales[SUPERRES_SCALES] = {
  { 8, 10 }, { 8, 12 }, { 8, 14 }, { 8, 16 }
};

const ScaleFactor *scale = &superres_scales[idx];
```
