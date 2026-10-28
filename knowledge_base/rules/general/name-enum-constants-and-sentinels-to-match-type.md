---
name: libaom/name-enum-constants-and-sentinels-to-match-type
description: When defining enum constants, sentinels, or invalid values, name them consistently with their enum type and ensure accompanying comments match the enum's domain concept
rubric: readability
severity: should_fix
---

**Rule.** Name enum constants—including sentinel, mask, and invalid
values—consistently with their enclosing enum type name and domain prefix.
Ensure accompanying comments accurately describe the enum's specific domain
concept rather than confusing distinct or sibling concepts (such as transform
type vs. transform size).

**Why.** Inconsistent constant names and inaccurate comments create ambiguity
about an enumerator's scope and meaning, increasing the risk that callers pass
values across mismatched enum types or domain concepts.

## Examples

Bad:

```cpp
enum {
  TX_TYPES,
  DCT_ADST_TX_MASK = 0x000F,
  TX_2D_INVALID = 255  // Invalid transform size
} UENUM1BYTE(TX_TYPE);
```

Good:

```cpp
enum {
  TX_TYPES,
  DCT_ADST_TX_MASK = 0x000F,
  TX_TYPE_INVALID = 255  // Invalid transform type
} UENUM1BYTE(TX_TYPE);
```
