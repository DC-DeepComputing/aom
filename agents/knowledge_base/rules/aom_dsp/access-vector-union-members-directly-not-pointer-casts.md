---
name: libaom/aom_dsp/access-vector-union-members-directly-not-pointer-casts
description: When reading or writing SIMD vector union types like c_v128 or c_v256, access union array members directly instead of casting the union address to pointer types
rubric: readability
severity: should_fix
---

**Rule.** When accessing, loading, or storing elements of SIMD vector union
types (such as `c_v64`, `c_v128`, or `c_v256`), access the typed union array
members (such as `a.u8[i]` or `a.u16[i]`) directly. Do not cast the address of
the vector union to a pointer (such as `(uint8_t *)&a`) to iterate over or index
its elements.

**Why.** Directly accessing defined union array members avoids redundant pointer
casting, clarifies the element type and width, and prevents potential
strict-aliasing issues or compiler `-Warray-bounds` warnings.

## Examples

Bad:

```cpp
uint8_t *q = (uint8_t *)&a;
for (int i = 0; i < 32; i++) {
  dst[i] = q[i];
}
```

Good:

```cpp
for (int i = 0; i < 32; i++) {
  dst[i] = a.u8[i];
}
```
