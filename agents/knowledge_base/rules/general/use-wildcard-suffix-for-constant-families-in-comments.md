---
name: libaom/use-wildcard-suffix-for-constant-families-in-comments
description: When referring collectively to a family of prefixed enums or macros in comments, use wildcard suffix notation like PREFIX_* rather than a non-existent base identifier
rubric: readability
severity: nit
---

**Rule.** When referring collectively to a family or group of enum constants,
macro definitions, or configuration flags sharing a common prefix in code
comments and documentation, append a wildcard suffix (e.g., PREFIX_* or
AOM_TUNE_VMAF_*). Do not cite a non-existent base identifier that looks like a
single defined constant.

**Why.** Citing an identifier that does not exist as a concrete constant or enum
value in the codebase is confusing and breaks symbol searches. Appending a
wildcard suffix clearly communicates to readers and API consumers that the
comment refers to an entire category or family of prefixed options rather than a
specific constant.

## Examples

Bad:

```cpp
// Bad: AOM_TUNE_VMAF does not exist as an individual enum constant.
/* Unlike metrics like AOM_TUNE_VMAF or AOM_TUNE_BUTTERAUGLI, */
```

Good:

```cpp
// Good: Wildcard suffix clearly indicates the family of related constants.
/* Unlike metrics like AOM_TUNE_VMAF_* or AOM_TUNE_BUTTERAUGLI, */
```
