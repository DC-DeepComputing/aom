---
name: libaom/use-aom-calloc-for-zero-initialized-allocations
description: When dynamically allocating memory that must be zero-initialized, use aom_calloc rather than calling aom_malloc followed by memset
rubric: readability
severity: nit
---

**Rule.** When allocating memory that needs to be zero-initialized, use
aom_calloc(num, size) (or calloc) rather than aom_malloc(size) followed by a
separate memset(ptr, 0, size) call.

**Why.** aom_calloc() allocates zero-initialized memory in a single operation,
eliminating redundant memset boilerplate, reducing code clutter, and preventing
bugs where memset is accidentally omitted or called with incorrect size
arguments.

## Examples

Bad:

```cpp
MB_MODE_INFO *tmp_mbmi = (MB_MODE_INFO *)aom_malloc(sizeof(*tmp_mbmi));
if (!tmp_mbmi) return AOM_CODEC_MEM_ERROR;
memset(tmp_mbmi, 0, sizeof(*tmp_mbmi));
```

Good:

```cpp
MB_MODE_INFO *tmp_mbmi = (MB_MODE_INFO *)aom_calloc(1, sizeof(*tmp_mbmi));
if (!tmp_mbmi) return AOM_CODEC_MEM_ERROR;
```
