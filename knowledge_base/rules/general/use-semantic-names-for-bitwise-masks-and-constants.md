---
name: libaom/use-semantic-names-for-bitwise-masks-and-constants
description: When defining bitwise masks or constants, name them according to their semantic purpose rather than generic operation-derived names like xor_val or and_mask
rubric: readability
severity: nit
---

**Rule.** Name bitwise masks, constants, and intermediate vector values
according to their semantic role or algorithmic purpose (such as `sign_mask`,
`sign_bit`, or `low_nibble_mask`) rather than mechanism- or operator-derived
names like `xor_val`, `and_mask`, or `or_val`. Operational names describe only
the instruction being executed rather than what the bit pattern represents in
the algorithm.

**Why.** Mechanism-based names like `xor_val` or `and_mask` repeat the operator
without explaining why the operation is performed or what value is being masked.
Semantic naming clarifies algorithmic intent (such as toggling the sign bit to
convert between signed and unsigned representations) and makes bitwise
manipulation easier to verify and maintain.

## Examples

Bad:

```cpp
const int8x8_t xor_val = vdup_n_s8(0x80);
pq_s0 = veor_s8(vreinterpret_s8_u8(*p0q0), xor_val);
```

Good:

```cpp
const int8x8_t sign_mask = vdup_n_s8(0x80);
pq_s0 = veor_s8(vreinterpret_s8_u8(*p0q0), sign_mask);
```
