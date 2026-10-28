---
name: libaom/omit-redundant-prefixes-in-local-variables
description: When declaring local variables holding struct members or block-scoped values, omit redundant subsystem, feature, or struct prefixes from variable names
rubric: readability
severity: nit
---

**Rule.** Do not include redundant subsystem, feature, module, or struct
prefixes in local variable names. Keep local variable names concise and focused
on their specific role (such as `exit` instead of `mb_wiener_mt_exit`), relying
on the enclosing function and block scope for context.

**Why.** Unlike global symbols or struct fields that require prefixes for
disambiguation across translation units, local variables have narrow function or
block scope. Repeating struct or feature prefixes on local variables adds
verbosity and visual clutter without adding clarity.

## Examples

Bad:

```cpp
// Redundant struct and subsystem prefixes on local variables
bool mb_wiener_mt_exit = enc_row_mt->mb_wiener_mt_exit;
int av1_cm_frame_width = cm->av1_frame_width;
```

Good:

```cpp
// Concise local variable names utilizing function scope context
const bool exit = enc_row_mt->mb_wiener_mt_exit;
const int frame_width = cm->av1_frame_width;
```
