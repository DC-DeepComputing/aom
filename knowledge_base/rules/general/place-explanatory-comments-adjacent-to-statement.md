---
name: libaom/place-explanatory-comments-adjacent-to-statement
description: Place explanatory comments and workaround rationales directly adjacent to the relevant statement, calculation, or conditional guard rather than at detached outer scopes
rubric: readability
severity: nit
---

**Rule.** Place explanatory comments, arithmetic rationales, and workaround
notes directly adjacent to the specific statement, calculation, conditional
check, preprocessor guard, or fallback implementation they describe. Do not
place comments detached at outer scopes, above unrelated loop headers, at the
start of a function, or near distant configuration macros. When documenting
compiler deficiencies, toolchain bugs, platform quirks, rounding/truncation
decisions, or branch conditions, position the explanation immediately preceding
the relevant code so maintainers can clearly associate the rationale with the
implementation.

**Why.** Separating explanatory comments from the specific statements,
calculations, or conditional guards they document—such as placing them above
outer loops or near unrelated macro definitions—obscures which operation or
fallback branch the rationale applies to. Keeping explanations and workaround
notes directly adjacent to the target code ensures immediate context during
review and prevents misunderstandings or accidental removals during refactoring.

## Examples

Bad:

```cpp
// Calculate subblock variances. Truncating values to integers was found to
// perform better than rounding, or returning them as doubles.
aom_variance_fn_t vf = cpi->ppi->fn_ptr[BLOCK_8X8].vf;
for (int subb_i = 0; subb_i < SUBBLOCKS_IN_SB_DIM; subb_i++) {
  int i = subb_i * SUBBLOCK_SIZE;
  for (int subb_j = 0; subb_j < SUBBLOCKS_IN_SB_DIM; subb_j++) {
    int j = subb_j * SUBBLOCK_SIZE;
    variances[subb_i * SUBBLOCKS_IN_SB_DIM + subb_j] =
        vf(x->plane[0].src.buf + i * x->plane[0].src.stride + j,
           x->plane[0].src.stride, all_zeros, 0, &sse) / 64;
  }
}

// Variant 2: Compiler workaround comment placed far from fallback guard
// Support for xN Neon intrinsics is lacking in some compilers.
#if defined(__arm__) || defined(_M_ARM)
#define ARM_32_BIT
#endif
// ...
#if defined(DEFICIENT_CLANG_32_BIT) || defined(DEFICIENT_GCC_32_BIT)
// Fallback implementation without xN intrinsics
#endif
```

Good:

```cpp
// Calculate subblock variances.
aom_variance_fn_t vf = cpi->ppi->fn_ptr[BLOCK_8X8].vf;
for (int subb_i = 0; subb_i < SUBBLOCKS_IN_SB_DIM; subb_i++) {
  int i = subb_i * SUBBLOCK_SIZE;
  for (int subb_j = 0; subb_j < SUBBLOCKS_IN_SB_DIM; subb_j++) {
    int j = subb_j * SUBBLOCK_SIZE;
    // Truncating values to integers (i.e. the 64 term) was found to perform
    // better than rounding, or returning them as doubles.
    variances[subb_i * SUBBLOCKS_IN_SB_DIM + subb_j] =
        vf(x->plane[0].src.buf + i * x->plane[0].src.stride + j,
           x->plane[0].src.stride, all_zeros, 0, &sse) / 64;
  }
}

// Variant 2: Workaround comment placed directly adjacent to the guard
#if defined(__arm__) || defined(_M_ARM)
#define ARM_32_BIT
#endif
// ...
// Support for xN Neon intrinsics is lacking in some compilers.
#if defined(DEFICIENT_CLANG_32_BIT) || defined(DEFICIENT_GCC_32_BIT)
// Fallback implementation without xN intrinsics
#endif
```
