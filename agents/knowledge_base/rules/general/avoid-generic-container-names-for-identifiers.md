---
name: libaom/avoid-generic-container-names-for-identifiers
description: When declaring struct members, fields, or variables, avoid bare container names or language keywords like stack or list, using descriptive domain-specific identifiers instead
rubric: readability
severity: nit
---

**Rule.** When naming struct members, fields, or local variables, avoid bare
generic container names or language tokens (such as stack, list, map, vector,
queue, or set) that trigger syntax highlighting or keyword collision in editors
and IDEs. Use descriptive, domain-specific identifiers (such as ref_mv_stack,
block_list, or symbol_map) that clarify the contents and role of the data
structure.

**Why.** Generic identifiers matching standard container types, keywords, or
common IDE tokens cause text editors and IDEs to highlight field names as
built-in types or language keywords, creating misleading syntax coloring.
Descriptive names clarify what entities the data structure holds and prevent
name collisions across scopes.

## Examples

Bad:

```cpp
typedef struct {
  uint8_t count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
} REF_MV_INFO;
```

Good:

```cpp
typedef struct {
  uint8_t count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
} REF_MV_INFO;
```
