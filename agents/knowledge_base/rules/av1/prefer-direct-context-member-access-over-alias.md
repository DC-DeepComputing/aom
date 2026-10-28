---
name: libaom/av1/prefer-direct-context-member-access-over-alias
description: When parent contexts (pbi or ppi) are in scope, access error state directly via pbi->error or ppi->error instead of sub-struct aliases like cm->error
rubric: readability
severity: should_fix
---

**Rule.** Access context members (such as error state) directly on the owning
top-level struct (`pbi->error` or `ppi->error`) rather than through alias
pointers in sub-structs (`cm->error`) when the parent context pointer is
available in scope. Reserve sub-struct alias pointers (`cm->error`) for shared
functions that only receive `AV1_COMMON *cm`.

**Why.** Direct access on the owning context struct avoids unnecessary pointer
indirection and potential null dereferences if the alias pointer is unset, and
keeps member access consistent with struct ownership hierarchies.

## Examples

Bad:

```cpp
if (setjmp(cm->error->jmp)) {
  cm->error->setjmp = 0;
  av1_decoder_remove(pbi);
  return NULL;
}
```

Good:

```cpp
if (setjmp(pbi->error.jmp)) {
  pbi->error.setjmp = 0;
  av1_decoder_remove(pbi);
  return NULL;
}
```
