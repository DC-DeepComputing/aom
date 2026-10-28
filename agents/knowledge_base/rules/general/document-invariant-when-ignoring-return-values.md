---
name: libaom/document-invariant-when-ignoring-return-values
description: When documenting why a fallible call or ignored return value is safe, explain the specific invariant or precondition guaranteeing safety rather than merely asserting safety.
rubric: readability
severity: nit
---

**Rule.** When documenting why a fallible function call's return value or error
status is safe to ignore without checking, explain the concrete invariant or
precondition that guarantees safety rather than merely asserting that it is
safe. State why failure or invalid state cannot occur in that specific call
path.

**Why.** A generic comment asserting that ignoring a return value is safe does
not explain to future maintainers why the function cannot fail or why error
handling is unnecessary. Documenting the specific invariant makes the safety
assumptions explicit and verifiable.

## Examples

Bad:

```cpp
// Ignoring the return value is safe when erasing the entry.
aom_film_grain_table_lookup(t, entry_end_time, end_time, /*erase=*/1,
                            NULL);
```

Good:

```cpp
// Ignoring the return value here is safe since we're erasing from the
// beginning of subsequent entries.
aom_film_grain_table_lookup(t, entry_end_time, end_time, /*erase=*/1,
                            NULL);
```
