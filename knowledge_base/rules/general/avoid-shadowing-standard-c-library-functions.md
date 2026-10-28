---
name: libaom/avoid-shadowing-standard-c-library-functions
description: When declaring local variables or parameters, avoid naming identifiers after standard C library functions like round or index to prevent -Wshadow warnings
rubric: readability
severity: should_fix
---

**Rule.** Do not name local variables, function parameters, or constants after
standard C library functions (such as `round`, `index`, `abs`, `exp`, `log`, or
`div`). Use descriptive alternatives such as `rounder`, `round_value`, `idx`, or
`abs_val` to avoid identifier collisions.

**Why.** Standard library headers such as `<math.h>` or `<stdlib.h>` declare
functions in the global scope. Naming local variables after these standard
functions triggers `-Wshadow` compiler warnings or build failures under strict
compiler settings and can obscure calls to standard functions.

## Examples

Bad:

```cpp
// Bad: 'round' shadows the standard C library math function round()
const __m128i round = _mm_set1_epi32(1 << (scale - 1));
const int index = get_index(pos);
```

Good:

```cpp
// Good: Use descriptive names that do not collide with standard library functions
const __m128i rounder = _mm_set1_epi32(1 << (scale - 1));
const int idx = get_index(pos);
```
