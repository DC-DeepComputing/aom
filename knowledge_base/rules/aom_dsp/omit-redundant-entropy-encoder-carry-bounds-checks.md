---
name: libaom/aom_dsp/omit-redundant-entropy-encoder-carry-bounds-checks
description: When implementing backwards carry propagation in arithmetic entropy encoders, omit redundant offset bounds checks and rely on carry terminating at or before the leading byte
rubric: readability
severity: nit
---

**Rule.** In arithmetic entropy encoder backwards carry propagation routines, do
not include redundant buffer index bounds checks (such as `offs > 0`) or
special-case branches for offset zero. Rely on the arithmetic coding invariant
that carry propagation always terminates at or before the leading byte.

**Why.** In arithmetic coding, the encoded value represents a probability in [0,
1). Adding a carry to the leading byte `buf[0]` can never produce another carry
because that would correspond to probability >= 1, making offset bounds checks
unreachable dead code.

## Examples

Bad:

```cpp
static AOM_INLINE void propagate_carry_bwd(unsigned char *buf, uint32_t offs) {
  uint16_t sum, carry = 1;
  while (carry && (offs > 0)) {
    sum = (uint16_t)buf[offs] + 1;
    buf[offs--] = (unsigned char)sum;
    carry = sum >> 8;
  }
  if (carry) {
    sum = (uint16_t)buf[offs] + 1;
    buf[offs] = (unsigned char)sum;
    assert(!(sum >> 8));
  }
}
```

Good:

```cpp
static AOM_INLINE void propagate_carry_bwd(unsigned char *buf, uint32_t offs) {
  uint16_t sum, carry = 1;
  do {
    sum = (uint16_t)buf[offs] + 1;
    buf[offs--] = (unsigned char)sum;
    carry = sum >> 8;
  } while (carry);
}
```
