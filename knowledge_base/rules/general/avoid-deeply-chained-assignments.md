---
name: libaom/avoid-deeply-chained-assignments
description: When updating multiple struct fields, pointer out-parameters, or distinct variables, avoid deeply chained assignment expressions and split them into separate statements
rubric: readability
severity: nit
---

**Rule.** Avoid deeply chained assignment expressions across multiple distinct
out-parameters, variables, and struct members (such as *a = *b = *c =
struct->field = val;). Split assignments into separate statements or clear,
logical steps matching surrounding code patterns.

**Why.** Chaining assignments across disparate out-parameters and nested struct
fields obscures data flow, impairs readability and step-through debugging, and
risks masking unintended conversions.

## Examples

Bad:

```cpp
// Bad: Chaining assignments across out-parameters and deep struct fields
*top_index = *bottom_index = *q = cpi->ppi->p_rc.arf_q =
    cpi->oxcf.rc_cfg.cq_level;
```

Good:

```cpp
// Good: Split into clear, distinct assignment statements
*q = cpi->oxcf.rc_cfg.cq_level;
*top_index = *bottom_index = *q;
cpi->ppi->p_rc.arf_q = *q;
```
