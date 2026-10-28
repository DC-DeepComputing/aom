---
name: libaom/avoid-doxygen-tags-in-non-doxygen-comments
description: When writing non-Doxygen comments or single-line // comments, use plain text instead of Doxygen command tags like \deprecated or @param
rubric: readability
severity: nit
---

**Rule.** Do not use Doxygen command tags (such as \deprecated, \param, \return,
or @param) in non-Doxygen comments like single-line // comments or non-doc /*
... */ blocks. Use plain text formatting (e.g., '// Deprecated. Use ...')
instead unless documenting public interfaces within a formal Doxygen comment
block (/** ... */ or /*! ... */).

**Why.** Doxygen command tags are only parsed within formal Doxygen
documentation blocks in public headers. Using Doxygen tags in standard
single-line or internal implementation comments does not generate documentation
and clutters the code with unrendered markup syntax.

## Examples

Bad:

```cpp
// \deprecated Use obu_memmove() instead.
static size_t obu_memmove_unsafe(uint8_t *dst, const uint8_t *src, size_t n);

// @param len Number of bytes to copy
void copy_buffer(uint8_t *dst, const uint8_t *src, size_t len);
```

Good:

```cpp
// Deprecated. Use obu_memmove() instead.
static size_t obu_memmove_unsafe(uint8_t *dst, const uint8_t *src, size_t n);

// len: Number of bytes to copy.
void copy_buffer(uint8_t *dst, const uint8_t *src, size_t len);
```
