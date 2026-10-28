---
name: libaom/standardize-terminology-for-related-identifiers
description: When naming parallel struct fields, parameters, or related local variables, use consistent terminology and abbreviation forms (e.g., count vs freq, _sz vs _size, _idx vs _index).
rubric: readability
severity: should_fix
---

**Rule.** When declaring or modifying related identifiers—such as parallel
struct members, function parameters, or interacting variables within the same
scope—use consistent naming terminology and abbreviation forms for the same
metric or concept. Do not mix divergent synonyms (such as count vs. freq, hint
vs. count, or offset vs. pos) or mix abbreviated and unabbreviated naming forms
(such as _sz vs. _size, _buf vs. _buffer, _idx vs. _index, or _len vs. _length).

**Why.** Mixing divergent synonyms or abbreviation forms across related
variables, parameters, or parallel struct fields obscures connections between
paired concepts, increases cognitive load, and misleads readers into assuming an
unintended semantic distinction.

## Examples

Bad:

```cpp
// Mixing synonyms 'freq' and 'count' for the same metric:
uint32_t value_freq[1 << 8] = { 0 };
uint32_t dominant_value_count = 0;

// Inconsistent terminology for parallel struct fields:
unsigned int order_hint;
unsigned int display_order_count;

// Mixing abbreviated _sz with unabbreviated _size in parameter list:
int av1_convert_sect5obus_to_annexb(uint8_t *buffer, size_t buffer_sz,
                                    size_t *input_size);
```

Good:

```cpp
// Consistent 'count' terminology across related variables:
uint32_t value_count[1 << 8] = { 0 };
uint32_t dominant_value_count = 0;

// Consistent terminology for parallel struct fields:
unsigned int order_hint;
unsigned int display_order_hint;

// Consistent unabbreviated parameter naming:
int av1_convert_sect5obus_to_annexb(uint8_t *buffer, size_t buffer_size,
                                    size_t *input_size);
```
