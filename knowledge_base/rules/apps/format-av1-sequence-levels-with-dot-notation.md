---
name: libaom/apps/format-av1-sequence-levels-with-dot-notation
description: When formatting AV1 sequence levels in log and warning messages, use '%d.%d' dot notation rather than underscore '%d_%d' to match the AV1 specification
rubric: readability
severity: nit
---

**Rule.** When formatting AV1 sequence level numbers or operating point levels
in log, warning, and error messages (such as in `aom_tools_warn` or
`aom_internal_error`), use dot notation (`%d.%d`, e.g., `2.0`, `5.1`) rather
than underscore separators (`%d_%d`), matching the notation defined in the AV1
specification.

**Why.** The AV1 specification designates operating point sequence levels using
major.minor notation (e.g., Level 2.0, Level 5.1). Using '%d.%d' in user-facing
logs and warning messages ensures consistency with standard specification
terminology and avoids confusing users with non-standard underscore notation.

## Examples

Bad:

```cpp
aom_tools_warn(
    "Failed to encode to target level %d_%d for operating point %d. "
    "The output level is %d_%d",
    2 + (target_levels[i] >> 2), target_levels[i] & 3, i,
    2 + (levels[i] >> 2), levels[i] & 3);
```

Good:

```cpp
aom_tools_warn(
    "Failed to encode to target level %d.%d for operating point %d. "
    "The output level is %d.%d",
    2 + (target_levels[i] >> 2), target_levels[i] & 3, i,
    2 + (levels[i] >> 2), levels[i] & 3);
```
