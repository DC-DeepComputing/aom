---
name: libaom/inline-constant-literals-in-format-strings
description: When formatting strings with printf or snprintf, embed constant characters and literal substrings directly into the format template instead of using format specifiers
rubric: readability
severity: nit
---

**Rule.** Embed constant characters and fixed literal strings directly into
printf/snprintf/fprintf format string templates rather than passing them through
%c or %s specifiers. Use format specifiers only for dynamic runtime values.

**Why.** Passing compile-time constant characters or strings as separate
arguments adds unnecessary parameter overhead and obscures the fixed structure
of the format template.

## Examples

Bad:

```cpp
snprintf(buf, len, "YUV4MPEG2 W%d H%d F%d:%d I%c%s %s\n", width, height,
         framerate->numerator, framerate->denominator, 'p', color_range,
         color);
```

Good:

```cpp
snprintf(buf, len, "YUV4MPEG2 W%d H%d F%d:%d Ip %s%s\n", width, height,
         framerate->numerator, framerate->denominator, color_range,
         color);
```
