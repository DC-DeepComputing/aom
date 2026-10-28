---
name: libaom/include-trailing-newline-in-printf-and-fprintf
description: When printing diagnostic, warning, or error messages with fprintf or printf, include required severity prefixes and trailing newlines
rubric: readability
severity: should_fix
---

**Rule.** When printing diagnostic, warning, or error messages using
fprintf(stderr, ...), fprintf(stdout, ...), printf(), or related formatted
output functions, ensure format strings include any required severity prefixes
(such as 'Warning: ') and end with a trailing newline ('\n'). Pay special
attention when replacing or converting from logging wrapper functions or macros
(such as warn() or aom_tools_warn()) that automatically prepend severity tags or
append newlines.

**Why.** Standard C I/O functions like printf and fprintf do not automatically
prepend severity labels or append newline characters. Omitting severity prefixes
loses critical diagnostic context, and omitting trailing '\n' leaves terminal
lines unterminated, causing subsequent warning, error, or progress logs to
concatenate on the same line and making console output difficult to parse.

## Examples

Bad:

```cpp
// Missing severity prefix and trailing newline when replacing warn():
fprintf(stderr, "Failed to read full frame");
fprintf(stderr, "Warning: Failed to allocate compressed data buffer");

if (!feof(infile)) {
  fprintf(stderr, "Failed to read frame size");
}
```

Good:

```cpp
// Format strings include explicit severity prefix and trailing newline:
fprintf(stderr, "Warning: Failed to read full frame\n");
fprintf(stderr, "Warning: Failed to allocate compressed data buffer\n");

if (!feof(infile)) {
  fprintf(stderr, "Failed to read frame size\n");
}
```
