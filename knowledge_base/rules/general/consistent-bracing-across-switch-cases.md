---
name: libaom/consistent-bracing-across-switch-cases
description: When writing switch statements where any case or default block uses curly braces, enclose all other case and default blocks in braces consistently
rubric: readability
severity: should_fix
---

**Rule.** In `switch` statements, if any `case` or `default` block is enclosed
in curly braces (`{}`), use curly braces consistently across all other `case`
and `default` blocks within the same switch. Do not mix braced and unbraced case
clauses in the same switch statement.

**Why.** Consistent bracing across all cases within a switch statement ensures
uniform structure and readability. Mixing braced clauses (often used to limit
the scope of local variable declarations) with unbraced single statements
creates visual inconsistency and makes block boundaries harder to scan.

## Examples

Bad:

```cpp
switch (mode) {
  case GLOBAL_GLOBALMV: {
    const int ref_frame = mbmi->ref_frame[0];
    process_global_mv(ref_frame);
    break;
  }
  default:
    return 0;
}
```

Good:

```cpp
switch (mode) {
  case GLOBAL_GLOBALMV: {
    const int ref_frame = mbmi->ref_frame[0];
    process_global_mv(ref_frame);
    break;
  }
  default: {
    return 0;
  }
}
```
