---
name: libaom/prefer-plain-int-over-fixed-width-integers
description: When declaring parameters, variables, or boolean flags where exact bit width is not required, use plain int rather than fixed-width types like uint8_t or int32_t
rubric: readability
severity: should_fix
---

**Rule.** Do not use fixed-width or narrow integer types (such as uint8_t,
int8_t, uint16_t, int32_t, or uint32_t) for function parameters, local
variables, or boolean/flag indicators (e.g., is_hbd, is_key_frame,
use_jnt_comp_avg) unless an exact bit width is strictly required by memory
layouts or hardware interfaces. Declare them as plain int to match standard C
conventions and API definitions.

**Why.** In C, function arguments and boolean flags are conventionally passed
and evaluated as int. Using narrow types like uint8_t or uint16_t provides no
memory savings for function parameters or local variables due to calling
convention register allocation and integer promotions, while introducing
unnecessary type casting and constraints at call sites. Similarly, using int32_t
or uint32_t adds artificial specificity and creates type inconsistencies when
interacting with standard APIs or struct fields defined as int.

## Examples

Bad:

```cpp
// Bad: Narrow types for boolean flag parameters
void aom_calc_ssim(const YV12_BUFFER_CONFIG *orig,
                   const YV12_BUFFER_CONFIG *recon,
                   const uint32_t bit_depth,
                   const uint32_t in_bit_depth,
                   uint8_t is_hbd,
                   double *weight,
                   double *frame_ssim2);

// Bad: int32_t for general function parameter or flag
static INLINE void compute_avg_4x1(
    uint16x4_t res0, uint16x4_t d0, const uint16_t fwd_offset,
    const uint16_t bck_offset, const int16x4_t sub_const_vec,
    const int16_t round_bits, const int32_t use_jnt_comp_avg, uint8x8_t *t0);
```

Good:

```cpp
// Good: Plain int for boolean flag parameter
void aom_calc_ssim(const YV12_BUFFER_CONFIG *orig,
                   const YV12_BUFFER_CONFIG *recon,
                   const uint32_t bit_depth,
                   const uint32_t in_bit_depth,
                   int is_hbd,
                   double *weight,
                   double *frame_ssim2);

// Good: Plain int for general function parameter or flag
static INLINE void compute_avg_4x1(
    uint16x4_t res0, uint16x4_t d0, const uint16_t fwd_offset,
    const uint16_t bck_offset, const int16x4_t sub_const_vec,
    const int16_t round_bits, const int use_jnt_comp_avg, uint8x8_t *t0);
```
