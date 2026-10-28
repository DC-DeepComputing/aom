---
name: libaom/match-parameter-const-between-declaration-and-definition
description: When modifying or declaring C function prototypes and definitions, ensure parameter const qualifiers match identically between headers and .c files to prevent MSVC compiler errors
rubric: correctness
severity: blocking
---

**Rule.** In C code, ensure function parameter const qualifiers match
identically between function declarations in headers (.h) and corresponding
definitions in source files (.c). When adding or removing const (including
top-level const on value parameters or pointers) in a header prototype or .c
definition, update both files synchronously.

**Why.** While GCC, Clang, and C++ compilers ignore differences in top-level
const qualifiers on value parameters between declarations and definitions,
MSVC's C compiler treats mismatched const qualifiers as an incompatible
redeclaration error (e.g. error C2371), breaking Windows builds.

## Examples

Bad:

```cpp
// av1/encoder/lookahead.h
struct lookahead_ctx *av1_lookahead_init(
    unsigned int width, unsigned int height, int use_highbitdepth,
    int depth, int border_in_pixels, int byte_alignment);

// av1/encoder/lookahead.c (mismatched top-level const fails on MSVC C compiler)
struct lookahead_ctx *av1_lookahead_init(
    unsigned int width, unsigned int height, int use_highbitdepth,
    int depth, const int border_in_pixels, int byte_alignment) {
  // ...
}
```

Good:

```cpp
// av1/encoder/lookahead.h
struct lookahead_ctx *av1_lookahead_init(
    unsigned int width, unsigned int height, int use_highbitdepth,
    int depth, int border_in_pixels, int byte_alignment);

// av1/encoder/lookahead.c (parameter qualifiers match header prototype identically)
struct lookahead_ctx *av1_lookahead_init(
    unsigned int width, unsigned int height, int use_highbitdepth,
    int depth, int border_in_pixels, int byte_alignment) {
  // ...
}
```
