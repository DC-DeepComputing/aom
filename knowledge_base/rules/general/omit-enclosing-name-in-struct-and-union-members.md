---
name: libaom/omit-enclosing-name-in-struct-and-union-members
description: When defining struct, class, or union members, omit enclosing type, role, or variable names from member identifiers to prevent redundant prefixes and suffixes
rubric: readability
severity: should_fix
---

**Rule.** When declaring members of a struct, class, or union, do not repeat the
enclosing type name, role, concept, or variable name as a prefix or suffix in
member identifiers (e.g., avoid compressor_data_alloc_width or cpi_ in AV1_COMP,
and avoid dclevel.dclevel16 or point.point_x). Name members directly after the
property, representation, or state they represent (such as data_alloc_width,
buf16/buf8, or x/y), relying on the enclosing container or variable instance to
provide scoping context at access and declaration sites.

**Why.** Struct, class, and union members are already scoped to their enclosing
container and accessed via its instance (e.g., cpi->data_alloc_width or
dclevel.buf16). Repeating the enclosing struct, union, or variable name within
member identifiers creates redundant stutter and verbosity without adding
clarity, cluttering both type definitions and member access sites across the
codebase.

## Examples

Bad:

```cpp
// Redundant struct name / role prefix in struct members
typedef struct AV1_COMP {
  int compressor_data_alloc_width;
  int compressor_data_alloc_height;
} AV1_COMP;

// Redundant enclosing variable/union name repetition in union members
union {
  DECLARE_ALIGNED(16, uint16_t, dclevel16[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, dclevel8[MAX_SB_SQUARE]);
} dclevel;
memset(dclevel.dclevel16, 0, sizeof(dclevel.dclevel16));
```

Good:

```cpp
// Concise member names relying on enclosing struct scope
typedef struct AV1_COMP {
  int data_alloc_width;
  int data_alloc_height;
} AV1_COMP;

// Clean member names without repeating the enclosing variable name
union {
  DECLARE_ALIGNED(16, uint16_t, buf16[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(16, uint8_t, buf8[MAX_SB_SQUARE]);
} dclevel;
memset(dclevel.buf16, 0, sizeof(dclevel.buf16));
```
