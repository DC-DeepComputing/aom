---
name: libaom/use-macro-parameters-not-enclosing-locals
description: When defining parameterized macros for code generation or specialization, ensure the macro body uses its formal parameters rather than enclosing local variables
rubric: correctness
severity: should_fix
---

**Rule.** When defining parameterized macros for code generation, kernel
unrolling, or compile-time specialization, ensure the macro expansion uses the
declared macro parameters rather than accidentally referencing enclosing local
variables from the caller's scope. Verify that every parameter in the macro
parameter list is referenced appropriately within the macro body.

**Why.** Inadvertently referencing enclosing local variables instead of the
formal macro parameters leaves the parameters unused and causes the macro to
operate on outer variables with fixed values. This defeats specialization,
prevents compile-time branch elimination, and can introduce subtle correctness
bugs when the macro is invoked across different contexts.

## Examples

Bad:

```cpp
// Macro takes USE_DIST_WEIGHTED but accidentally references local var use_dist_wtd_comp_avg
#define DO_AVG_2D_COPY(USE_DIST_WEIGHTED)                                    \
  {                                                                          \
    const __m256i res_0 =                                                    \
        comp_avg(&ref_0, &src_0, &wt, use_dist_wtd_comp_avg);               \
    _mm256_storeu_si256((__m256i *)(dst + 0 * dst_stride), res_0);          \
  }
```

Good:

```cpp
// Macro correctly uses the formal parameter USE_DIST_WEIGHTED
#define DO_AVG_2D_COPY(USE_DIST_WEIGHTED)                                    \
  {                                                                          \
    const __m256i res_0 =                                                    \
        comp_avg(&ref_0, &src_0, &wt, (USE_DIST_WEIGHTED));                  \
    _mm256_storeu_si256((__m256i *)(dst + 0 * dst_stride), res_0);          \
  }
```
