---
name: libaom/define-single-use-constants-and-macros-in-function-scope
description: When defining helper macros or constants used only within a single function, define them inside function scope and omit redundant prefixes
rubric: readability
severity: nit
---

**Rule.** Define named constants and helper macros in the narrowest scope
possible, placing single-use definitions directly inside the consuming function
body rather than at file or header scope. Once scoped locally to a function,
omit redundant feature, subsystem, or function prefixes from their identifier
names.

**Why.** Placing single-use constants and macros at file or header scope
pollutes translation-unit namespaces and obscures where values are consumed.
Restricting their scope to the single function where they are used clarifies
local intent, prevents accidental reuse across the translation unit, and enables
cleaner, shorter identifier names without redundant prefixes.

## Examples

Bad:

```cpp
// At file scope in aq_variance.c:
#define VAR_BOOST_SUBBLOCKS_IN_SB_DIM 8
#define VAR_BOOST_SUBBLOCK_SIZE 8

unsigned int av1_get_block_variance_boost(const AV1_COMP *cpi,
                                          const MACROBLOCK *x) {
  for (int subb_i = 0; subb_i < VAR_BOOST_SUBBLOCKS_IN_SB_DIM; subb_i++) {
    int i = subb_i * VAR_BOOST_SUBBLOCK_SIZE;
    // ...
  }
}
```

Good:

```cpp
unsigned int av1_get_block_variance_boost(const AV1_COMP *cpi,
                                          const MACROBLOCK *x) {
#define SUBBLOCKS_IN_SB_DIM 8
#define SUBBLOCKS_IN_SB 64
#define SUBBLOCK_SIZE 8
  for (int subb_i = 0; subb_i < SUBBLOCKS_IN_SB_DIM; subb_i++) {
    int i = subb_i * SUBBLOCK_SIZE;
    // ...
  }
}
```
