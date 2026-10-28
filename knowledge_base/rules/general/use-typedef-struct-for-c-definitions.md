---
name: libaom/use-typedef-struct-for-c-definitions
description: When defining structs in C, use anonymous typedef struct declarations without repeating struct tags unless needed for self-referential pointers or forward declarations
rubric: readability
severity: nit
---

**Rule.** Define C structs using 'typedef struct { ... } TypeName;' rather than
standalone 'struct TypeName { ... };', and reference the typedef name directly
in function signatures, parameter lists, and variable declarations without
repeating the 'struct' keyword. In struct typedefs, use an anonymous struct
('typedef struct { ... } TypeName;') without repeating the name in a struct tag
unless a tag is specifically required for forward declarations or
self-referential member pointers (such as linked list or tree node pointers).

**Why.** In C, struct tags reside in a separate tag namespace, requiring the
'struct' keyword on every variable or parameter declaration unless typedefed.
Using 'typedef struct' simplifies type usage across function signatures and
source files. Furthermore, unless a struct tag is required for self-referential
pointer members or forward declarations, defining an anonymous struct avoids
redundant duplicate identifier declarations and keeps type definitions concise.

## Examples

Bad:

```cpp
// Bad: Standalone struct definition requiring 'struct' keyword
struct SphereMV {
  double phi;
  double theta;
};

void av1_motion_search_brute_force_erp(..., struct SphereMV *best_mv);

// Bad: Duplicating the struct tag name in a non-self-referential typedef
typedef struct CONVOLVE_OPS {
  const float **input;
  int in_width;
  int in_height;
} CONVOLVE_OPS;
```

Good:

```cpp
// Good: Anonymous struct typedef used directly without 'struct' keyword
typedef struct {
  double phi;
  double theta;
} SphereMV;

void av1_motion_search_brute_force_erp(..., SphereMV *best_mv);

typedef struct {
  const float **input;
  int in_width;
  int in_height;
} CONVOLVE_OPS;

// Struct tags remain appropriate when self-referencing pointers are required:
typedef struct TreeNode {
  int value;
  struct TreeNode *left;
  struct TreeNode *right;
} TreeNode;
```
