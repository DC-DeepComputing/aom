---
name: libaom/init-target-struct-first-parameter
description: When defining helper functions that initialize a struct or context (`init_*`), pass the target struct pointer as the first parameter and avoid top-level `const` qualifiers.
rubric: readability
severity: nit
---

**Rule.** In helper functions that initialize a struct or context (`init_*`),
pass the pointer to the struct being initialized as the first parameter (acting
as the destination/receiver object). Additionally, omit redundant top-level
`const` qualifiers on pointer parameters in function signatures (use `T *param`
rather than `T *const param`).

**Why.** Placing the target struct first conforms to destination-first and
object-receiver conventions used across libaom initialization helpers, ensuring
predictable argument order. Top-level const qualifiers on pointer parameters in
function declarations add visual clutter without changing interface semantics
for callers.

## Examples

Bad:

```cpp
static INLINE void init_estimate_block_intra_args(
    AV1_COMP *cpi, MACROBLOCK *x,
    struct estimate_block_intra_args *const args);
```

Good:

```cpp
static INLINE void init_estimate_block_intra_args(
    struct estimate_block_intra_args *args, AV1_COMP *cpi, MACROBLOCK *x);
```
