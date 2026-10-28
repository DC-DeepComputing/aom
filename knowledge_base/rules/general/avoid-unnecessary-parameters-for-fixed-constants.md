---
name: libaom/avoid-unnecessary-parameters-for-fixed-constants
description: Avoid parameterizing helper functions and macros with invariant constants, types, casts, or specification-fixed values; hardcode or define them internally
rubric: design
severity: should_fix
---

**Rule.** Do not add parameters to helper functions or preprocessor macros for
values (such as fixed constants, specification-defined values, element types, or
type casts) that are invariant across all call sites or invocations. Instead,
keep signatures and macro definitions minimal by hardcoding constants,
referencing standard definitions, or defining local constants internally
(documenting invariants with a concise comment).

**Why.** Over-parameterizing functions and macros with invariant values
introduces unnecessary API surface complexity, creates visual clutter and
boilerplate across call sites, adds argument-passing overhead, and gives a false
impression that the behavior is configurable when it is actually fixed.

## Examples

Bad:

```cpp
// Variant 1: Passing invariant parameters or spec-fixed constants to helper functions
static INLINE uint8x8_t lpf_flat_mask4(uint8x8_t p3q3, uint8x8_t p2q2,
                                       uint8x8_t p1q1, uint8x8_t p0q0,
                                       const uint8_t thresh) {
  const uint8x8_t thresh_8x8 = vdup_n_u8(thresh);
  ...
}
// Call site: flat_8x8 = lpf_flat_mask4(*p3q3, *p2q2, *p1q1, *p0q0, 1);

void av1_calculate_scaled_size(int *width, int *height, int resize_denom,
                               int resize_nom);

// Variant 2: Macro parameterizing invariant casts or types across all calls
#define AOM_SUB_PIXEL_VAR(w, h, wf, wlog2, hlog2, cast_prod, cast) \
  ... cast_prod(cast se * se) ...

AOM_SUB_PIXEL_VAR(128, 128, 32, 7, 7, (int64_t), (int64_t));
AOM_SUB_PIXEL_VAR(128, 64, 32, 7, 6, (int64_t), (int64_t));
```

Good:

```cpp
// Variant 1: Fixed values defined internally, keeping signatures minimal
static inline uint8x8_t lpf_flat_mask4(uint8x8_t p3q3, uint8x8_t p2q2,
                                       uint8x8_t p1q1, uint8x8_t p0q0) {
  const uint8x8_t thresh_8x8 = vdup_n_u8(1);  // for bd==8 threshold is always 1
  ...
}
// Call site: flat_8x8 = lpf_flat_mask4(*p3q3, *p2q2, *p1q1, *p0q0);

void av1_calculate_scaled_size(int *width, int *height, int resize_denom);

// Variant 2: Invariant types and casts hardcoded inside the macro
#define AOM_SUB_PIXEL_VAR(w, h, wf, wlog2, hlog2) \
  ... ((int64_t)se * se) ...

AOM_SUB_PIXEL_VAR(128, 128, 32, 7, 7);
AOM_SUB_PIXEL_VAR(128, 64, 32, 7, 6);
```
