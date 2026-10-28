---
name: libaom/update-associated-macros-and-constants-when-renaming-identif
description: When renaming or expanding abbreviated data structures, tables, or variables, update associated macros, constants, and helper definitions to maintain consistent prefix and naming conventions
rubric: readability
severity: should_fix
---

**Rule.** When renaming, expanding abbreviations in, or refactoring identifiers
(such as lookup tables, arrays, structs, or functions), update all associated
macros, constants, enumerators, and helper definitions to match the new naming
and prefix conventions. Do not leave stale or abbreviated prefixes on companion
macros and constants when the underlying data structure or type has been
expanded or renamed.

**Why.** Inconsistent naming between associated macros, constants, and data
structures hurts code readability, creates confusion about symbol relationships,
and makes codebase-wide text and symbol searches unreliable.

## Examples

Bad:

```cpp
// Table was renamed to smooth_weights, but macro retains old abbreviated prefix
#define SM_WEIGHT_LOG2_SCALE 8
static const uint8_t smooth_weights[] = { ... };
```

Good:

```cpp
// Macro updated to match the expanded table name and prefix
#define SMOOTH_WEIGHT_LOG2_SCALE 8
static const uint8_t smooth_weights[] = { ... };
```
