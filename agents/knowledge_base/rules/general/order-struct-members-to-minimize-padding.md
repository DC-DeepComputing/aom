---
name: libaom/order-struct-members-to-minimize-padding
description: When defining or modifying struct fields, group smaller-width types together or order members by decreasing alignment to minimize struct padding bytes
rubric: safety
severity: nit
---

**Rule.** When declaring or modifying fields in a struct or class, group
smaller-width members (such as bool, uint8_t, int8_t, and uint16_t) together or
order fields by descending alignment and size. Avoid interleaving larger-aligned
fields (such as 32-bit/64-bit integers or pointers) between narrow types where
alignment requirements introduce unnecessary internal or trailing padding.

**Why.** Compilers align struct members to their natural boundaries, inserting
padding bytes whenever a larger-aligned field follows an unaligned narrow field.
Reordering struct fields or grouping narrow types together reduces memory
overhead and improves cache locality, especially for frequently allocated or
embedded structs.

## Examples

Bad:

```cpp
struct DepthRepresentationElement {
  bool sign_flag;
  uint8_t exponent;
  uint32_t mantissa;     // Incurs 2 padding bytes before and 3 after
  uint8_t mantissa_len;
};
```

Good:

```cpp
struct DepthRepresentationElement {
  bool sign_flag;
  uint8_t exponent;
  uint8_t mantissa_len;  // Grouped with narrow fields to minimize padding
  uint32_t mantissa;
};
```
