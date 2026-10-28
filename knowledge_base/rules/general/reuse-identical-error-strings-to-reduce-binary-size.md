---
name: libaom/reuse-identical-error-strings-to-reduce-binary-size
description: When reporting errors or validation failures, reuse existing identical error message string literals instead of introducing unique format strings to reduce binary size
rubric: safety
severity: should_fix
---

**Rule.** When reporting errors or validation failures (such as via
aom_internal_error() or aom_set_error()), reuse existing identical error message
string literals rather than introducing new unique format strings for similar
error conditions (such as integer or size_t overflow checks). Avoid unique
format specifiers or distinct phrasing when a standardized message suffices.

**Why.** Compilers and linkers merge identical string literals into a single
entry in read-only data (.rodata). Reusing identical error message strings
across similar failure checks enables string deduplication and prevents library
binary size bloat.

## Examples

Bad:

```cpp
if ((size_t)size > SIZE_MAX / (size_t)total_max_sb_square) {
  aom_internal_error(cm->error, AOM_CODEC_ERROR,
                     "%d * %d would overflow size_t", size,
                     total_max_sb_square);
}
```

Good:

```cpp
if ((size_t)size > SIZE_MAX / (size_t)total_max_sb_square) {
  aom_internal_error(cm->error, AOM_CODEC_ERROR,
                     "A multiplication would overflow size_t");
}
```
