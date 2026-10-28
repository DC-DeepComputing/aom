---
name: libaom/avoid-redundant-local-parameter-aliases
description: When accessing or stepping through function parameters, use or advance parameters directly instead of creating redundant local pointer aliases, temporary copies, or no-op type casts
rubric: readability
severity: should_fix
---

**Rule.** Do not declare redundant local pointer alias variables, temporary
pointer copies (such as const uint8_t *ref_tmp = ref), or no-op type casts to
re-wrap function parameters when the parameter already matches the required type
and const-qualification. Reference parameters directly throughout the function
body, or advance pass-by-value pointer parameters directly (e.g., ref += stride
or ptr++) when stepping through buffers. Retain a local temporary pointer copy
only when the initial pointer address must be preserved for subsequent passes or
resets within the function.

**Why.** In C and C++, pointer parameters are passed by value and modifying them
inside the function body does not affect the caller's arguments. Creating local
alias variables, no-op casts, or temporary stepping pointers adds unnecessary
indirection, clutters local scopes with duplicate identifiers, and obscures that
the parameter itself can serve directly as the working pointer or reference.

## Examples

Bad:

```cpp
// Bad: Redundant local pointer alias with no-op cast of matching parameter
void aom_smooth_h_predictor_64x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[63]);
  ...
}

// Bad: Redundant temporary pointer alias for stepping through buffer
static INLINE void aom_int_pro_col_16wd_avx2(int16_t *vbuf, const uint8_t *ref,
                                             const int ref_stride,
                                             const int height,
                                             int norm_factor) {
  const uint8_t *ref_tmp = ref;
  for (int ht = 0; ht < height; ht += 8) {
    process_column(vbuf, ref_tmp, norm_factor);
    ref_tmp += (ref_stride << 3);
  }
}
```

Good:

```cpp
// Good: Access parameter directly without intermediate alias or cast
void aom_smooth_h_predictor_64x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i top_right = _mm_set1_epi16(top_row[63]);
  ...
}

// Good: Advance pass-by-value pointer parameter directly
static INLINE void aom_int_pro_col_16wd_avx2(int16_t *vbuf, const uint8_t *ref,
                                             const int ref_stride,
                                             const int height,
                                             int norm_factor) {
  for (int ht = 0; ht < height; ht += 8) {
    process_column(vbuf, ref, norm_factor);
    ref += (ref_stride << 3);
  }
}
```
