---
name: libaom/use-descriptive-names-over-nonstandard-abbreviations
description: When naming helper functions, SIMD routines, or variables, use clear descriptive terminology like partial_sum rather than non-standard abbreviations or colloquialisms like redux
rubric: readability
severity: nit
---

**Rule.** Avoid non-standard abbreviations, slang, or colloquialisms (such as
'redux') in function, helper, and variable names. Use clear, standard
descriptive terminology (such as 'partial_sum' or 'partial_reduction') that
explicitly conveys the intermediate or partial nature of the operation.

**Why.** Non-standard abbreviations and colloquial terms obscure function intent
and hinder readability across architectures. Standard descriptive naming clearly
communicates algorithmic semantics, such as clarifying that an intermediate
vector reduction or accumulation step was performed before final reduction.

## Examples

Bad:

```cpp
static INLINE uint16x8_t dc_load_redux_32(const uint8_t *in) {
  const uint8x16_t a0 = vld1q_u8(in);
  const uint8x16_t a1 = vld1q_u8(in + 16);
  return vpadalq_u8(vpaddlq_u8(a0), a1);
}
```

Good:

```cpp
static INLINE uint16x8_t dc_load_partial_sum_32(const uint8_t *in) {
  const uint8x16_t a0 = vld1q_u8(in);
  const uint8x16_t a1 = vld1q_u8(in + 16);
  return vpadalq_u8(vpaddlq_u8(a0), a1);
}
```
