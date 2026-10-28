---
name: libaom/prefer-logging-total-tries-over-computed-retries
description: 'When logging diagnostic or error messages after retry loops, report total attempt counts (''tries: %d'') directly rather than computed retries (''count - 1'').'
rubric: readability
severity: nit
---

**Rule.** When printing diagnostic, warning, or error messages in retry loops or
multi-attempt operations, report the total attempt count directly (e.g., 'tries:
%d', passing try_count) rather than computing retries via subtraction
expressions (e.g., 'retries: %d', passing try_count - 1). Avoid arithmetic
adjustments on attempt counters in log formatting arguments.

**Why.** Passing computed retries like try_count - 1 introduces unnecessary
mental overhead for readers verifying that try_count > 0 and risks outputting
confusing negative numbers (such as retries: -1) if an error occurs before the
first attempt or if the loop exits with 0 attempts. Logging the raw attempt
counter directly as total tries is unambiguous, simpler, and robust against
zero-attempt edge cases.

## Examples

Bad:

```cpp
// Anti-pattern: computing retries with try_count - 1 risks printing negative values if try_count is 0.
fprintf(stderr,
        "Error reading file: %u of %u bytes read, error: %d, retries: %d\n",
        len, size, file_error, try_count - 1);
```

Good:

```cpp
// Recommended: log total attempt count directly using the raw counter.
fprintf(stderr,
        "Error reading file: %u of %u bytes read, error: %d, tries: %d\n",
        len, size, file_error, try_count);
```
