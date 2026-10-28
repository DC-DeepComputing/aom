---
name: libaom/match-commit-descriptions-to-code-identifiers
description: When referencing code identifiers in commit messages, ensure variable names, functions, and parameters match the exact code symbols rather than approximate or informal names.
rubric: readability
severity: nit
---

**Rule.** When describing changes in the commit message, verify that all
referenced identifiers, variable names, struct fields, functions, and constants
accurately match the actual code symbols modified or used in the patch set. Do
not use colloquial, approximate, or outdated names in place of the exact code
identifiers.

**Why.** Referencing imprecise or incorrect identifier names in commit messages
causes confusion during code review, misleads readers investigating historical
changes in git log or blame, and makes searching commit history for specific
symbols unreliable.

## Examples

Bad:

```cpp
Use the source_sad for setting the refresh map, with modified thresholds for temporal layers.
```

Good:

```cpp
Use the superblock source SAD (sb_sad) for setting the refresh map, with modified thresholds for base TL0.
```
