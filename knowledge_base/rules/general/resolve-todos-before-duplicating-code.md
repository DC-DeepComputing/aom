---
name: libaom/resolve-todos-before-duplicating-code
description: When porting or duplicating code containing existing TODO comments, check if the proposed improvement can be resolved directly rather than copying over unresolved technical debt
rubric: readability
severity: should_fix
---

**Rule.** When porting, adapting, or duplicating code from another file,
library, or subsystem, do not copy over existing TODO comments as-is without
investigating whether they can be resolved. Address the pending optimization or
refactoring in the new implementation rather than propagating known technical
debt.

**Why.** Copying unresolved TODO comments and known suboptimal implementations
multiplies technical debt across codebases. Investigating and resolving pending
improvements during porting ensures new code is optimal from the start and
avoids lingering maintenance burdens.

## Examples

Bad:

```cpp
// Duplicating existing code with its unresolved TODO comment as-is.
// TODO(johannkoenig): cle() should work with vmovn(top_left_dist) instead of
// using movl(x_dist).
const uint8x8_t x_le_top_left_low =
    vmovn_u16(vcleq_u16(vmovl_u8(vget_low_u8(x_dist)), top_left_dist_low));
```

Good:

```cpp
// Resolving the pending optimization when adding the new function.
const uint8x16_t top_left_dist = vcombine_u8(vqmovn_u16(top_left_dist_low),
                                             vqmovn_u16(top_left_dist_high));
return vcleq_u8(x_dist, top_left_dist);
```
