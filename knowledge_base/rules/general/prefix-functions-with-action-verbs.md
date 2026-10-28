---
name: libaom/prefix-functions-with-action-verbs
description: When naming functions and helper routines, prefix identifiers with an action verb such as get_ rather than using bare noun phrases
rubric: readability
severity: nit
---

**Rule.** Name functions and helper routines with an action or verb prefix (such
as get_ for accessor/lookup functions, set_ for mutators, or compute_/calc_ for
calculations) rather than bare noun phrases that look like variables or data
tables. In static helpers where module prefixes like av1_ are omitted,
explicitly include an action verb to distinguish the callable function from
local variables and arrays.

**Why.** In C codebases where identifiers use snake_case, bare noun phrases
typically denote variables, structs, or lookup tables. Prefixing function names
with action verbs clarifies at call sites that an operation is being performed
and avoids name collisions or confusion with data structures.

## Examples

Bad:

```cpp
// Bare noun phrase looks like a variable or lookup table array
static const uint8_t *var_offs(int use_hbd, int bd) {
  if (use_hbd) return high_var_offs[bd];
  return low_var_offs;
}
```

Good:

```cpp
// Action verb 'get_' clarifies that this is an accessor helper function
static const uint8_t *get_var_offs(int use_hbd, int bd) {
  if (use_hbd) return high_var_offs[bd];
  return low_var_offs;
}
```
