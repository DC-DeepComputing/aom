---
name: libaom/order-specialized-before-general-branches
description: When writing if-else chains with overlapping conditions, order specialized subcase branches before general branches to avoid redundant negated conditions
rubric: readability
severity: nit
---

**Rule.** In `if`/`else if` chains where one branch handles a specialized subset
of a broader case, evaluate the specialized branch before the general branch. Do
not place the general branch first with redundant negated sub-conditions just to
allow the specialized case to fall through.

**Why.** Evaluating specialized conditions first simplifies the conditional flow
and avoids complex negated clauses in subsequent general branches, improving
readability and maintainability.

## Examples

Bad:

```cpp
// General branch placed first requires negated specialized conditions
if (mode == NEWMV && !force_inter &&
    (!x->sb_me_block || ref_frame > LAST_FRAME)) {
  handle_general_newmv(x, mode);
} else if (x->sb_me_block && mode == NEWMV && ref_frame == LAST_FRAME) {
  handle_specialized_sb_me(x, mode);
}
```

Good:

```cpp
// Specialized branch placed first allows concise general fallback
if (x->sb_me_block && mode == NEWMV && ref_frame == LAST_FRAME) {
  handle_specialized_sb_me(x, mode);
} else if (mode == NEWMV && !force_inter) {
  handle_general_newmv(x, mode);
}
```
