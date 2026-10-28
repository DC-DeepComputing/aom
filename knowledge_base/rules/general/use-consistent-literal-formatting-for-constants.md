---
name: libaom/use-consistent-literal-formatting-for-constants
description: When writing integer constants, bit patterns, or vector broadcast values, maintain consistent literal formatting (such as hexadecimal versus decimal) across related functions and files
rubric: readability
severity: nit
---

**Rule.** Maintain consistent literal formatting (such as hexadecimal 0x80
versus decimal 128 for bit masks or byte broadcast values) across related
functions within the same file or subsystem. When nearby functions or sibling
implementations use a specific literal representation for a constant value,
adhere to that established convention rather than mixing numeric bases.

**Why.** Inconsistent literal representations (such as mixing decimal 128 and
hexadecimal 0x80 for identical byte values) across related functions impair
readability, obscure symmetry between sibling routines, and make text searches
across the codebase less reliable.

## Examples

Bad:

```cpp
// In a file where sibling DC predictors use 0x80 for byte broadcast:
dc_store_4xh(dst, stride, 8, vdup_n_u8(128));
```

Good:

```cpp
// Match the established hexadecimal literal format used in sibling functions:
dc_store_4xh(dst, stride, 8, vdup_n_u8(0x80));
```
