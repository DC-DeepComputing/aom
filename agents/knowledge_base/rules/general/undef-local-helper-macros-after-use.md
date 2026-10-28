---
name: libaom/undef-local-helper-macros-after-use
description: When defining local, internal, or template helper macros in source or header files, undefine (#undef) every macro immediately after use
rubric: readability
severity: should_fix
---

**Rule.** Undefine locally defined helper, internal, and token-pasting template
macros using `#undef MACRO_NAME` immediately after their intended block,
function, or section of use, and before the end of header or template files.
Ensure every helper macro defined in a file is explicitly undefined so none leak
past their intended scope.

**Why.** Preprocessor macros lack lexical scoping and ignore language visibility
rules, persisting across the entire translation unit and any subsequent included
files. Leaving local helper or template macros defined risks macro namespace
pollution, unintended expansions, and symbol collisions or compiler redefinition
warnings across repeated inclusions.

## Examples

Bad:

```cpp
// Variant 1: Local helper macro left defined after function
#define APPLY_HORIZONTAL_SHIFT_4X1(fn, ...) \
  do { fn(__VA_ARGS__); } while (0)
static INLINE void process_block(...) {
  APPLY_HORIZONTAL_SHIFT_4X1(shift_row, ...);
}
// Missing #undef APPLY_HORIZONTAL_SHIFT_4X1

// Variant 2: Internal/template helper macros left defined in header
#define K_MEANS_RENAME_C(x, y) x##_dim##y##_c
#define RENAME_C(x) K_MEANS_RENAME_C(x, AV1_K_MEANS_DIM)
// ... template definitions ...
#undef RENAME_C
// Missing #undef for K_MEANS_RENAME_C
```

Good:

```cpp
// Variant 1: Undefined immediately after use
#define APPLY_HORIZONTAL_SHIFT_4X1(fn, ...) \
  do { fn(__VA_ARGS__); } while (0)
static INLINE void process_block(...) {
  APPLY_HORIZONTAL_SHIFT_4X1(shift_row, ...);
}
#undef APPLY_HORIZONTAL_SHIFT_4X1

// Variant 2: All template and helper macros cleaned up before EOF
#define K_MEANS_RENAME_C(x, y) x##_dim##y##_c
#define RENAME_C(x) K_MEANS_RENAME_C(x, AV1_K_MEANS_DIM)
// ... template definitions ...
#undef RENAME_C
#undef K_MEANS_RENAME_C
```
