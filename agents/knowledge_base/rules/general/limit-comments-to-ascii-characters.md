---
name: libaom/limit-comments-to-ascii-characters
description: When writing source code comments, use standard ASCII characters and punctuation instead of non-ASCII Unicode characters like em-dashes or arrows
rubric: readability
severity: nit
---

**Rule.** Limit source code comments to standard ASCII characters. Do not use
non-ASCII Unicode characters or punctuation (such as em-dashes '—', en-dashes
'–', arrows '→', or smart quotes '“”'); use plain ASCII equivalents (such as
'-', '--', '->', or '"') instead.

**Why.** Non-ASCII Unicode characters can cause text encoding, rendering, and
terminal display inconsistencies across different developer tools, operating
systems, and command-line environments. Using standard ASCII ensures universal
portability and visual consistency across the codebase.

## Examples

Bad:

```cpp
// Encode SL0 only — mi_cols stays stale. SAD loop overruns it → OOB.
/* Compute step: x₀ = a · b → clamp to range [min … max] */
```

Good:

```cpp
// Encode SL0 only - mi_cols stays stale. SAD loop overruns it -> OOB.
/* Compute step: x0 = a * b -> clamp to range [min ... max] */
```
