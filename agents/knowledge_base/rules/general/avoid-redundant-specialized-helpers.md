---
name: libaom/avoid-redundant-specialized-helpers
description: When implementing helper functions for fixed arguments, constants, or sign variations, call a generalized parameterized helper with explicit arguments instead of defining redundant specialized wrappers
rubric: design
severity: should_fix
---

**Rule.** Do not define separate, single-purpose helper functions or duplicate
variants that merely hardcode a constant, enum value, or arithmetic sign (+/-)
instead of accepting a parameter. Consolidate such routines into a single
generalized helper accepting parameterized (or signed) arguments, and pass the
explicit constant (e.g., LAST_FRAME), enum, or signed value (e.g., -cospi[...]
or coef) directly from call sites.

**Why.** Creating dedicated single-purpose wrappers or separate
positive/negative variants that mirror a generalized helper's implementation
bloats internal APIs, increases maintenance overhead across architecture ports
(e.g., NEON/SSE transforms), and risks logic divergence when updates or bug
fixes are applied to one function but missed in another.

## Examples

Bad:

```cpp
// Bad: Redundant helper hardcoding LAST_FRAME
static INLINE int calculate_ref_params_scaling_base(AV1_COMMON *const cm) {
  const RefCntBuffer *const buf = get_ref_frame_buf(cm, LAST_FRAME);
  const int last_poc = buf ? (int)buf->absolute_poc : -1;
  return last_poc - cm->cur_frame->absolute_poc;
}

// Bad: Separate helpers differing only by coefficient sign
static INLINE void btf_16_neon(const int16x8_t in0, int16_t coef1,
                               int16_t coef2, int32x4_t *s0, int32x4_t *s1) {
  *s0 = vmull_n_s16(vget_low_s16(in0), coef1);
  *s1 = vmull_n_s16(vget_low_s16(in0), -coef2);
}
static INLINE void btf_16_neon_pos(const int16x8_t in0, int16_t coef1,
                                   int16_t coef2, int32x4_t *s0, int32x4_t *s1) {
  *s0 = vmull_n_s16(vget_low_s16(in0), coef1);
  *s1 = vmull_n_s16(vget_low_s16(in0), coef2);
}
```

Good:

```cpp
// Good: Keep single generalized parameterized helpers
static INLINE int calculate_ref_params_scaling_distance(AV1_COMMON *const cm,
                                                        int frame) {
  const RefCntBuffer *const buf = get_ref_frame_buf(cm, frame);
  const int ref_poc = buf ? (int)buf->absolute_poc : -1;
  return ref_poc - cm->cur_frame->absolute_poc;
}

static INLINE void btf_16_neon(const int16x8_t in0, int16_t coef1,
                               int16_t coef2, int32x4_t *s0, int32x4_t *s1) {
  *s0 = vmull_n_s16(vget_low_s16(in0), coef1);
  *s1 = vmull_n_s16(vget_low_s16(in0), coef2);
}

// Callers pass explicit constants, enums, or signed values directly:
int base = calculate_ref_params_scaling_distance(cm, LAST_FRAME);
btf_16_neon(x[1], cospi[60], -cospi[4], &s0, &s1);
btf_16_neon(in[1], cospi[60], cospi[4], &step2[8], &step2[15]);
```
