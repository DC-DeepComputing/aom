---
name: libaom/save-repeated-vector-lane-extractions-in-local-variable
description: When reusing a vector lane extraction across multiple operations, store the extracted value in a local variable instead of duplicating the intrinsic call
rubric: readability
severity: nit
---

**Rule.** When extracting a vector lane or scalar value using SIMD intrinsics
(such as vget_lane_*, vgetq_lane_*, or _mm_extract_epi*) for reuse across
multiple statements or operations, store the extracted scalar in a named local
variable. Do not duplicate identical getter calls across multiple expressions or
setter operations.

**Why.** Extracting repeated subexpressions into a descriptive local variable
eliminates redundant nested calls, clarifies code intent, and simplifies
debugging without altering compiler code generation in optimized builds.

## Examples

Bad:

```cpp
// Duplicating the lane extraction intrinsic across multiple setters
s = vset_lane_u8(vget_lane_u8(s, 2), s, 0);
s = vset_lane_u8(vget_lane_u8(s, 2), s, 1);
```

Good:

```cpp
// Storing the extracted lane in a const local variable for reuse
const uint8_t pad = vget_lane_u8(s, 2);
s = vset_lane_u8(pad, s, 0);
s = vset_lane_u8(pad, s, 1);
```
