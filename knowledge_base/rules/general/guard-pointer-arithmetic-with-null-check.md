---
name: libaom/guard-pointer-arithmetic-with-null-check
description: When adjusting optional or nullable buffer pointers by an offset, verify the pointer is non-null before pointer arithmetic instead of checking only non-zero offset
rubric: safety
severity: should_fix
---

**Rule.** When adjusting an optional or conditionally allocated buffer pointer
by an offset, verify that the pointer is non-null before performing pointer
arithmetic rather than checking only that the offset is non-zero. If an offset
is non-zero when the buffer pointer is NULL, handle the invalid state by
returning an error code or asserting non-nullness.

**Why.** Performing pointer arithmetic on a NULL pointer is undefined behavior
in C. Checking only that the offset is non-zero fails to protect against adding
offsets to an uninitialized NULL pointer and obscures error handling when a
non-zero payload unexpectedly lacks a destination buffer.

## Examples

Bad:

```cpp
// Anti-pattern: Only checking if offset is non-zero before pointer arithmetic
if (length_field != 0) {
  saved_wb.bit_buffer += length_field;
}
```

Good:

```cpp
// Recommended: Verify pointer is non-null, and handle unexpected non-zero offset
if (saved_wb.bit_buffer != NULL) {
  saved_wb.bit_buffer += length_field;
} else if (length_field != 0) {
  return AOM_CODEC_ERROR;
}
```
