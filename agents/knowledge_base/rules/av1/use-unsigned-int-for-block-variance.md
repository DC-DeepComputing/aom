---
name: libaom/av1/use-unsigned-int-for-block-variance
description: When declaring variables, arrays, or helper functions storing or returning block variance, use unsigned int matching aom_variance_fn_t
rubric: readability
severity: should_fix
---

**Rule.** When declaring variables, arrays, parameters, or helper functions that
store, compute, or return block variance values, declare them as `unsigned int`
to match the `aom_variance_fn_t` (and `aom_variance_fn_ptr_t::vf`) DSP function
pointer return type. Do not use signed `int` for variance values or
variance-returning helper functions.

**Why.** DSP variance calculation function pointers (`aom_variance_fn_t` / `vf`)
compute non-negative sums of squared differences and return `unsigned int`.
Using signed `int` introduces signedness mismatches against DSP primitives,
triggers `-Wsign-compare` compiler warnings, and risks integer overflow or
incorrect sign comparisons.

## Examples

Bad:

```cpp
// BAD: Storing or returning block variance values as signed int
int variances[SUBBLOCKS_IN_SB];
aom_variance_fn_t vf = cpi->ppi->fn_ptr[BLOCK_8X8].vf;
variances[i] = vf(src, src_stride, ref, ref_stride, &sse);

int av1_get_block_variance_boost(const AV1_COMP *cpi, const MACROBLOCK *x);
```

Good:

```cpp
// GOOD: Use unsigned int matching the aom_variance_fn_t return type
unsigned int variances[SUBBLOCKS_IN_SB];
aom_variance_fn_t vf = cpi->ppi->fn_ptr[BLOCK_8X8].vf;
variances[i] = vf(src, src_stride, ref, ref_stride, &sse);

unsigned int av1_get_block_variance_boost(const AV1_COMP *cpi,
                                          const MACROBLOCK *x);
```
