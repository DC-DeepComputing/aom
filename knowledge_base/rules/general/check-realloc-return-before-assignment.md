---
name: libaom/check-realloc-return-before-assignment
description: When resizing memory with realloc, assign to a temporary pointer and check for NULL before updating the original pointer to prevent memory leaks
rubric: safety
severity: should_fix
---

**Rule.** When resizing memory with realloc(), do not assign the return value
directly back to the original pointer (e.g., ptr = realloc(ptr, size)). Assign
the result to a temporary pointer variable first, verify that it is non-NULL,
and only then update the original pointer.

**Why.** If realloc() fails, it returns NULL without freeing the original memory
block. Overwriting the original pointer directly with the return value destroys
the reference to the existing allocation, leaking memory when error handling
recovers or continues.

## Examples

Bad:

```cpp
// Overwrites syms with NULL on failure, leaking original memory.
accounting->syms.syms =
    realloc(accounting->syms.syms,
            sizeof(AccountingSymbol) * accounting->num_syms_allocated);
if (!accounting->syms.syms) return -1;
```

Good:

```cpp
AccountingSymbol *new_syms =
    realloc(accounting->syms.syms,
            sizeof(AccountingSymbol) * accounting->num_syms_allocated);
if (!new_syms) return -1;
accounting->syms.syms = new_syms;
```
