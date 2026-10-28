---
name: libaom/plural-suffix-for-multi-bit-masks
description: When naming bitmask macros or constants representing multiple bits or composite flags, use plural suffixes like *_BITS or *_MASK instead of *_BIT.
rubric: readability
severity: nit
---

**Rule.** When defining macros or constants for bitmasks that represent multiple
bits, multiple bit positions, or composite flag combinations, use plural
suffixes such as *_BITS or *_MASK rather than singular *_BIT. Maintain
consistent plural naming across related bitmask definitions within the same
header or macro family.

**Why.** Using a singular *_BIT suffix is misleading when the mask represents
multiple combined bits or composite feature flags (such as combined CPUID
feature flags). Plural suffixes like *_BITS or *_MASK clarify that the value
contains multiple bits and prevent incorrect assumptions at call sites.

## Examples

Bad:

```cpp
// Misleading singular suffix for composite multi-bit masks
#define AVX_BIT (BIT(27) | BIT(28))
#define AVX512_BIT (BIT(16) | BIT(17) | BIT(28) | BIT(30) | BIT(31))
#define MMX_BIT BIT(23)
```

Good:

```cpp
// Plural suffixes accurately convey multi-bit masks and maintain consistency
#define AVX_BITS (BIT(27) | BIT(28))
#define AVX512_BITS (BIT(16) | BIT(17) | BIT(28) | BIT(30) | BIT(31))
#define MMX_BITS BIT(23)
```
