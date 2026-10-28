---
name: libaom/init-non-null-terminated-char-arrays-with-char-list
description: When defining fixed-size character arrays without a null terminator such as magic headers, initialize them with explicit character lists instead of string literals
rubric: readability
severity: nit
---

**Rule.** When defining a fixed-size character or byte array that is not
null-terminated (such as a file magic signature or binary header), initialize it
with an explicit character list ({ 'a', 'b', ... }) matching the exact size
rather than a string literal.

**Why.** String literals implicitly append a trailing null terminator ('\0').
Sizing a non-null-terminated array to accommodate the string literal introduces
an unwanted extra byte and distorts sizeof, while sizing it to the exact length
causes compiler errors in C++ or truncation warnings in C.

## Examples

Bad:

```cpp
// Bad: String literal requires a 9th byte for '\0', skewing sizeof.
static const char kFileMagic[9] = "filmgrn1";
```

Good:

```cpp
// Good: Explicit character list matches the exact 8-byte header.
static const char kFileMagic[8] = { 'f', 'i', 'l', 'm', 'g', 'r', 'n', '1' };
```
