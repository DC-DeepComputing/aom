---
name: libaom/avoid-redundant-guards-for-idempotent-assignments
description: When sequential or nested conditionals guard idempotent assignments, omit redundant inverse guards and simplify control flow or combine checks using logical OR
rubric: readability
severity: nit
---

**Rule.** Do not add redundant nested conditional checks (such as `if
(!cond_b)`) inside an outer `if (cond_a)` block solely to avoid repeating an
idempotent assignment that is also performed by a subsequent `if (cond_b)`
check. Either permit the harmless duplicate assignment across both sequential
checks or combine the conditions into a single unified conditional such as `if
(cond_a || cond_b)`.

**Why.** Adding nested inverse condition guards to prevent redundant writes of
identical values increases nesting depth and cognitive complexity without
altering program semantics. Simplifying the control flow or merging conditions
with logical OR makes the intent explicit and easier to maintain.

## Examples

Bad:

```cpp
// Bad: Redundant nested check (!is_low_chroma_strength) added solely to avoid writing 0 twice
if (is_low_luma_strength) {
  cdef_info->cdef_strengths[j] = 0;
  if (!is_low_chroma_strength) {
    cdef_info->cdef_uv_strengths[j] = 0;
  }
}
if (is_low_chroma_strength) {
  cdef_info->cdef_uv_strengths[j] = 0;
}
```

Good:

```cpp
// Good: Simplify by allowing the harmless idempotent assignment or combining checks with ||
if (is_low_luma_strength) {
  cdef_info->cdef_strengths[j] = 0;
  cdef_info->cdef_uv_strengths[j] = 0;
}
if (is_low_chroma_strength) {
  cdef_info->cdef_uv_strengths[j] = 0;
}

// Or combine the condition if setting chroma strength unconditionally on either event:
if (is_low_luma_strength || is_low_chroma_strength) {
  cdef_info->cdef_uv_strengths[j] = 0;
}
```
