---
name: libaom/avoid-redundant-initialization-with-exhaustive-conditionals
description: Omit redundant initializations for local variables or struct fields unconditionally assigned across exhaustive control branches, or initialize default values and omit fallback branches.
rubric: readability
severity: should_fix
---

**Rule.** Do not redundantly initialize local variables or composite struct
fields (e.g., SIMD vector structs or scalar locals) when all subsequent control
flow branches unconditionally overwrite them, such as in exhaustive if/else or
switch constructs, or when silencing compiler warnings like
-Wmaybe-uninitialized. Either declare the variable or struct field uninitialized
prior to the exhaustive conditional block, or initialize it with the default
value and omit the redundant fallback branch (else or default). Only
pre-initialize specific fields or variables that have genuine execution paths
where they remain unassigned.

**Why.** Redundant initializations introduce dead stores that compiler
optimizations may not always eliminate in hot DSP/SIMD routines. They also
obscure data flow and intent by concealing which branches actually assign
values. Conversely, retaining fallback else branches that re-assign an already
initialized default value adds unnecessary code noise and branching.

## Examples

Bad:

```cpp
// Redundant initialization before exhaustive branches
int max_lf_sharpness = 0;
if (cm->quant_params.base_qindex <= 120) {
  max_lf_sharpness = 7;
} else if (cm->quant_params.base_qindex <= 160) {
  max_lf_sharpness = 1;
} else {
  max_lf_sharpness = 0;
}

// Redundantly initializing SIMD struct fields unconditionally assigned
uint16x4x2_t v_shift;
v_shift.val[0] = vdup_n_u16(0);  // Dead store: assigned in both branches
v_shift.val[1] = vdup_n_u16(0);
if (condition) {
  v_shift.val[0] = vdup_n_u16(shift_a);
  v_shift.val[1] = vdup_n_u16(shift_b);
} else {
  v_shift.val[0] = vdup_n_u16(shift_c);
}
```

Good:

```cpp
// Declare uninitialized when branches are exhaustive, or omit fallback else
int max_lf_sharpness;
if (cm->quant_params.base_qindex <= 120) {
  max_lf_sharpness = 7;
} else if (cm->quant_params.base_qindex <= 160) {
  max_lf_sharpness = 1;
} else {
  max_lf_sharpness = 0;
}

// Only initialize fields that are conditionally omitted
uint16x4x2_t v_shift;
v_shift.val[1] = vdup_n_u16(0);  // val[0] omitted because both branches set it
if (condition) {
  v_shift.val[0] = vdup_n_u16(shift_a);
  v_shift.val[1] = vdup_n_u16(shift_b);
} else {
  v_shift.val[0] = vdup_n_u16(shift_c);
}
```
