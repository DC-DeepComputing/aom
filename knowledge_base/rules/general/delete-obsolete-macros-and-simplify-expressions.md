---
name: libaom/delete-obsolete-macros-and-simplify-expressions
description: 'When refactoring, inlining, or retiring features, delete obsolete macros, unused enum values, constants, typedefs, static lookup tables, and orphaned #undefs, and simplify expressions'
rubric: readability
severity: should_fix
---

**Rule.** When refactoring algorithms, data structures, enums, or bitstream
representations, inlining logic, or retiring architecture backends and
deprecated features:
1. Delete all obsolete or unreferenced `#define` macros, bit-width constants, enum values (including `UENUM1BYTE` flags), `typedef` definitions, type aliases, function pointer types (`*_fn`), and local helper declarations.
2. Remove unreferenced static lookup tables, dispatch arrays (`*_arr`, `*_tab`, `*_table`), and helper variables instead of leaving dead declarations in source files.
3. Remove matching `#undef` cleanup directives when their corresponding `#define` macro definitions are removed.
4. Simplify remaining expressions across call sites by removing zero-shift arithmetic or identity operations.
5. Remove redundant preprocessor conditionals when `#if` and `#else` branches become identical, and add compile-time `#error` guards if a shared header must not be included for a retired architecture target.

**Why.** Leaving dead symbols, unused enum values, unreferenced static lookup
tables, obsolete typedefs/macros, orphaned #undef directives, or duplicate
#if/#else branches creates dead code, clutters headers and source files, and can
trigger compiler unused-variable warnings. It also misleads maintainers about
supported operating modes, configurations, or active abstractions, risking
silent misuse of legacy constants or misleading calculations.

## Examples

Bad:

```cpp
// Variant 1: Obsolete bit macro, zero-shift expression, and orphaned #undef:
#define CDF_SHIFT (15 - CDF_PROB_BITS)  // CDF_PROB_BITS is now fixed to 15.
v = (r >> 8) >> (7 - EC_PROB_SHIFT - CDF_SHIFT);
#undef CDF_SHIFT

// Variant 2: Unused enum flag in header:
enum {
  FRAMEFLAGS_ERROR_RESILIENT = 1 << 6,
  FRAMEFLAGS_PSNR = 1 << 7,  // Unreferenced anywhere in codebase
} UENUM1BYTE(FRAMETYPE_FLAGS);

// Variant 3: Dead typedef after inlining function pointer dispatch:
typedef float (*activation_fn)(float);

// Variant 4: Unreferenced static lookup table left in source file:
static const transform_1d_neon lowbd_txfm_w4_arr[TX_SIZES][ITX_TYPES_1D] = { ... };

// Variant 5: Redundant preprocessor conditional with identical branches:
#if CONFIG_REALTIME_ONLY
#define SAD_MXNX4D(m, n) aom_sad##m##x##n##x4d_c(...)
#else
#define SAD_MXNX4D(m, n) aom_sad##m##x##n##x4d_c(...)
#endif
```

Good:

```cpp
// Variant 1: Obsolete macro and #undef deleted, shift simplified:
v = (r >> 8) >> (7 - EC_PROB_SHIFT);

// Variant 2: Only actively referenced enum flags retained:
enum {
  FRAMEFLAGS_ERROR_RESILIENT = 1 << 6,
} UENUM1BYTE(FRAMETYPE_FLAGS);

// Variant 3: Obsolete activation_fn typedef deleted during refactoring.

// Variant 4: Unreferenced static lookup table removed entirely.

// Variant 5: Preprocessor conditional removed when branches are identical:
#define SAD_MXNX4D(m, n) aom_sad##m##x##n##x4d_c(...)
```
