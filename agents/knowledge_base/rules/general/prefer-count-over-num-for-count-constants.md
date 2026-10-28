---
name: libaom/prefer-count-over-num-for-count-constants
description: When defining constants, macros, or enum sentinels representing a total count or capacity, use the _COUNT suffix rather than _NUM
rubric: readability
severity: should_fix
---

**Rule.** Use the '_COUNT' suffix (or 'COUNT' in identifier names) rather than
'_NUM' for constants, macro definitions, and enum sentinels that represent a
total count of elements, modes, or items.

**Why.** 'COUNT' unambiguously denotes a cardinal total quantity of items or
modes, whereas 'NUM' is ambiguous and easily conflated with an index, sequence
number, or ordinal identifier.

## Examples

Bad:

```cpp
// Ambiguous '_NUM' suffix for a total count of modes or elements
#define LUMA_MODE_NUM 61

enum {
  FRAME_LF_NUM = 4,
};
```

Good:

```cpp
// Clear '_COUNT' suffix for a total count of modes or elements
#define LUMA_MODE_COUNT 61

enum {
  FRAME_LF_COUNT = 4,
};
```
