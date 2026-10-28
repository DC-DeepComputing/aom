---
name: libaom/name-enums-after-selected-attribute-or-variant
description: When defining enumeration types or UENUM1BYTE definitions, name the enum after the category, variant, or attribute selected rather than temporal or adjacent terms
rubric: readability
severity: should_fix
---

**Rule.** When defining enumeration types (including UENUM1BYTE definitions or
typedef enum), name the enum type after the specific attribute, variant,
category, or operation it selects (such as *_TYPE, *_MODE, *_FORMAT, or
*_VARIANT). Avoid ambiguous temporal suffixes (such as *_TIME or *_WHEN) or
names of adjacent concepts whose behavior does not vary across the enumerators.

**Why.** Enum type names define the semantic domain of their enumerator values.
Naming an enum after temporal attributes or unrelated concepts obscures what
configuration is being selected and misleads readers about what the values
represent.

## Examples

Bad:

```cpp
enum {
  COPY_NONE,
  COPY_INPUT,
  COPY_OUTPUT,
  COPY_COMBINED
} UENUM1BYTE(COPY_TIME);
```

Good:

```cpp
enum {
  COPY_NONE,
  COPY_INPUT,
  COPY_OUTPUT,
  COPY_COMBINED
} UENUM1BYTE(COPY_TYPE);
```
