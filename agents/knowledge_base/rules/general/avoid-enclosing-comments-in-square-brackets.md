---
name: libaom/avoid-enclosing-comments-in-square-brackets
description: When writing inline or trailing explanatory comments, write standard descriptive prose without enclosing the comment text in square brackets
rubric: readability
severity: nit
---

**Rule.** Do not enclose inline or trailing explanatory comments in square
brackets (`[...]`). Write descriptive comments as standard prose without
enclosing brackets, reserving brackets for mathematical intervals (such as
`[min, max]`) or SIMD vector lane layouts.

**Why.** Enclosing plain prose comments in square brackets adds unnecessary
punctuation and departs from standard comment formatting. In code comments,
square brackets are conventionally reserved for mathematical intervals, array
index notations, or SIMD register lane layouts.

## Examples

Bad:

```cpp
uint32_t *block_hash_values[2] = { NULL };  // [two buffers used ping-pong]
```

Good:

```cpp
uint32_t *block_hash_values[2] = { NULL };  // two buffers used ping-pong
```
