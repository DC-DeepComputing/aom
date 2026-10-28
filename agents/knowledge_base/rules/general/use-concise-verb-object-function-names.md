---
name: libaom/use-concise-verb-object-function-names
description: When naming functions or helper routines, use concise, natural verb-object phrases rather than awkward compound names joined by conjunctions like and
rubric: readability
severity: nit
---

**Rule.** Name functions and helper routines with concise, natural verb-object
phrases that directly describe the operation (such as buffer_input). Avoid
awkward compound names joined by conjunctions (such as _and_ in
buffer_and_input) or redundant phrasing when a direct verb-noun form expresses
the action cleanly.

**Why.** Awkward compound names with conjunctions add unnecessary verbosity and
make call sites clunky to read. Concise verb-object phrases clearly communicate
the function's primary action while adhering to idiomatic C/C++ naming
conventions.

## Examples

Bad:

```cpp
// Awkward compound function name with conjunction
size_t buffer_and_input(struct AvxInputContext *input_ctx, size_t n,
                        unsigned char *buf, bool buffered);
```

Good:

```cpp
// Concise, natural verb-object function name
size_t buffer_input(struct AvxInputContext *input_ctx, size_t n,
                    unsigned char *buf, bool buffered);
```
