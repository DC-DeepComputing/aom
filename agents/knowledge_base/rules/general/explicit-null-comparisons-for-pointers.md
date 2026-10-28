---
name: libaom/explicit-null-comparisons-for-pointers
description: When checking pointer nullness in conditions or logical expressions, use explicit comparisons against NULL (ptr != NULL or ptr == NULL) instead of implicit boolean conversions
rubric: readability
severity: should_fix
---

**Rule.** When checking pointer nullness or validity in conditional statements
and logical expressions, explicitly compare pointers against NULL (e.g., ptr !=
NULL or ptr == NULL). Avoid relying on implicit boolean evaluations (e.g., if
(ptr) or if (!ptr)).

**Why.** Explicit comparisons against NULL clearly distinguish pointer nullness
checks from boolean flags or integer conditions and maintain consistent coding
style across the codebase.

## Examples

Bad:

```cpp
if (ref_stamp >= 0 && ref_buf[LAST2_FRAME - LAST_FRAME]) {
  process_reference(ref_buf[LAST2_FRAME - LAST_FRAME]);
}
if (!ctx->buffer_pool) {
  return AOM_CODEC_MEM_ERROR;
}
```

Good:

```cpp
if (ref_stamp >= 0 && ref_buf[LAST2_FRAME - LAST_FRAME] != NULL) {
  process_reference(ref_buf[LAST2_FRAME - LAST_FRAME]);
}
if (ctx->buffer_pool == NULL) {
  return AOM_CODEC_MEM_ERROR;
}
```
