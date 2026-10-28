---
name: libaom/avoid-referencing-foreign-types-in-header-docs
description: When documenting types, structs, or functions in header comments, describe their role directly rather than referencing non-existent external or foreign abstractions
rubric: readability
severity: should_fix
---

**Rule.** In header and API documentation comments, describe types, structs,
enums, and functions directly in terms of their role within the library rather
than referencing external, foreign, or ported abstractions (such as 'Foo in C'
or language wrapper equivalents) that do not exist in the codebase.

**Why.** Documentation comments in headers are the primary reference for library
consumers and maintainers. Referencing non-existent external types,
foreign-language classes, or out-of-tree wrappers confuses readers who have no
context for those foreign abstractions and makes documentation unhelpful and
obsolete.

## Examples

Bad:

```cpp
/*!\brief GopFrame in C.
 */
typedef struct aom_rc_gop_frame {
  int frame_idx;
} aom_rc_gop_frame_t;
```

Good:

```cpp
/*!\brief A single frame in the GOP (group of pictures).
 */
typedef struct aom_rc_gop_frame {
  int frame_idx;
} aom_rc_gop_frame_t;
```
