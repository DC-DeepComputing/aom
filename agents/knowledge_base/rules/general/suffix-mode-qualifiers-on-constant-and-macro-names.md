---
name: libaom/suffix-mode-qualifiers-on-constant-and-macro-names
description: When defining mode-specific or specialized variants of constants and macros, append the variant qualifier as a suffix rather than inserting it into the base identifier
rubric: readability
severity: nit
---

**Rule.** When defining specialized, feature-specific, or mode-dependent
variants of existing constants, enum values, or macros, append the mode or
variant qualifier as a suffix to the base identifier (such as BASE_NAME_MODE)
rather than inserting it in the middle (such as BASE_MODE_NAME). Maintain the
intact base prefix across all related variants to preserve searchability,
autocompletion, and logical grouping.

**Why.** Keeping the base constant or macro prefix intact ensures that IDE
autocompletions and codebase-wide text searches for the base identifier (e.g.,
DEFAULT_QM_FIRST) immediately discover both the default definition and all
mode-specific variations (such as DEFAULT_QM_FIRST_ALLINTRA). Splitting or
reordering base tokens fractures lexical grouping and makes related
configuration limits harder to find and audit.

## Examples

Bad:

```cpp
// Bad: Mode qualifier ('ALLINTRA') is inserted into the middle of the base identifier.
#define DEFAULT_QM_FIRST 5
#define DEFAULT_QM_LAST 9
#define DEFAULT_QM_ALLINTRA_FIRST 4
#define DEFAULT_QM_ALLINTRA_LAST 10
```

Good:

```cpp
// Good: Mode qualifier is placed as a suffix, keeping the base identifier intact.
#define DEFAULT_QM_FIRST 5
#define DEFAULT_QM_LAST 9
#define DEFAULT_QM_FIRST_ALLINTRA 4
#define DEFAULT_QM_LAST_ALLINTRA 10
```
