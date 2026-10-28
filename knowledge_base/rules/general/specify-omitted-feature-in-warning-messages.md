---
name: libaom/specify-omitted-feature-in-warning-messages
description: When printing a warning for a non-fatal failure before continuing execution, explicitly specify which feature is disabled rather than printing a generic 'Continuing...'
rubric: readability
severity: nit
---

**Rule.** When logging or printing a warning message upon encountering a
non-fatal failure (such as an optional buffer allocation or configuration
parsing failure) and continuing execution, explicitly state which feature, tool,
or behavior is being skipped or disabled. Avoid vague or generic messages like
'Continuing...' that leave the user or developer unaware of what functionality
was omitted.

**Why.** Generic messages such as 'Continuing...' obscure the operational impact
of a non-fatal failure. Explicitly stating what feature is bypassed makes
warning diagnostics actionable and helps users determine why expected outputs or
behaviors are missing.

## Examples

Bad:

```cpp
fprintf(stderr,
        "Warning: unable to allocate histogram buffers. Continuing...\n");
```

Good:

```cpp
fprintf(stderr,
        "Warning: Unable to allocate buffers required for "
        "show_rate_histogram().\n"
        "Continuing without rate histogram feature...\n");
```
