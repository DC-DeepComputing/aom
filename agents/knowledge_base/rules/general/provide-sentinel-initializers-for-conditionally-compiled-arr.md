---
name: libaom/provide-sentinel-initializers-for-conditionally-compiled-arr
description: 'When conditionally populating elements of a fixed-size array using preprocessor guards, provide explicit sentinel initializers in the #else branch when zero-initialization is invalid'
rubric: correctness
severity: should_fix
---

**Rule.** When conditionally initializing elements of a fixed-size array using
`#if CONFIG_...` preprocessor guards, always provide an `#else` branch
containing explicit sentinel entries (e.g., `UNDEFINED_LEVEL`) if C default
zero-initialization does not match the sentinel state. Do not leave
conditionally omitted array elements to be implicitly zero-initialized when
index 0 or all-zero fields represent valid data.

**Why.** In C, omitted elements in a fixed-size array initializer are implicitly
initialized to zero. When zero represents a valid enum value or active data
structure rather than an invalid sentinel, accesses to conditionally omitted
slots will read misleading valid data instead of detecting disabled elements.

## Examples

Bad:

```cpp
static const AV1LevelSpec av1_level_defs[SEQ_LEVELS] = {
  // Level definitions up to 6.3...
#if CONFIG_CWG_C013
  { .level = SEQ_LEVEL_7_0, .max_pic_size = 141557760, ... },
  // Levels 7.0 to 8.3...
#endif
  // Omitted elements default to all-zeros, erroneously mapping to SEQ_LEVEL_2_0.
};
```

Good:

```cpp
static const AV1LevelSpec av1_level_defs[SEQ_LEVELS] = {
  // Level definitions up to 6.3...
#if CONFIG_CWG_C013
  { .level = SEQ_LEVEL_7_0, .max_pic_size = 141557760, ... },
  // Levels 7.0 to 8.3...
#else  // !CONFIG_CWG_C013
  UNDEFINED_LEVEL,
  UNDEFINED_LEVEL,
  // ...
#endif
};
```
