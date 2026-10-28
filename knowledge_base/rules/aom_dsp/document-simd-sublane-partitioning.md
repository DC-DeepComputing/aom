---
name: libaom/aom_dsp/document-simd-sublane-partitioning
description: When defining non-trivial SIMD helpers, transposes, or reduction templates, document sub-lane partitioning semantics and illustrate input-to-output element rearrangements with diagram comments
rubric: readability
severity: nit
---

**Rule.** When declaring or defining non-trivial SIMD vector helper functions,
intrinsic wrappers, matrix transposes, or template utilities (such as
transpose_* helpers, block-wise reductions like BlockReduceSum, vector
interleaves/permutations, or dual-lane shuffles), precede the function with
descriptive doc comments and element mapping diagrams. Specifically:
- For SIMD matrix transposes, vector interleaves, and permutations (e.g., using vtrn, vzip, vuzp, _mm_unpacklo/hi, or _mm_shuffle), provide an illustrative diagram detailing the input vector element ordering and the resulting output vector element layout.
- For sub-lane partitioned operations (e.g., dual 128-bit lanes within 256-bit vectors like c_v256_pshuffle_8), explicitly clarify whether the operation executes within independent sub-lanes or across full vector boundaries.
- For multi-block reductions or hierarchical lane transformations, include concrete input and output vector lane layout examples illustrating the reduction arithmetic.

**Why.** SIMD vector manipulations—including matrix transposes, sub-lane
intrinsic wrappers, vector interleaves, and block-wise reductions—involve
non-obvious lane boundaries, multidimensional permutations, and hierarchical
arithmetic that are difficult to verify at a glance. Illustrative comments,
sub-lane boundary notes, and input/output layout diagrams make element
rearrangements readable, clarify in-lane vs. cross-lane semantics, simplify code
auditing, and prevent subtle index permutation bugs.

## Examples

Bad:

```cpp
// Bad: missing sub-lane boundary explanation
SIMD_INLINE c_v256 c_v256_pshuffle_8(c_v256 a, c_v256 pattern) {
  return c_v256_from_v128(
      c_v128_shuffle_8(c_v256_high_v128(a), c_v256_high_v128(pattern)),
      c_v128_shuffle_8(c_v256_low_v128(a), c_v256_low_v128(pattern)));
}

// Bad: missing input-to-output element layout diagram
static inline void transpose_arrays_s64_4x4(const int64x2_t *in,
                                            int64x2_t *out) {
  out[0] = aom_vtrn1q_s64(in[0], in[2]);
  out[1] = aom_vtrn1q_s64(in[4], in[6]);
  out[2] = aom_vtrn2q_s64(in[0], in[2]);
  out[3] = aom_vtrn2q_s64(in[4], in[6]);
}
```

Good:

```cpp
// Good: explains dual 128-bit sub-lane partitioning
// Pairwise / dual-lane shuffle: shuffles elements within two independent 128-bit lanes.
SIMD_INLINE c_v256 c_v256_pshuffle_8(c_v256 a, c_v256 pattern) {
  return c_v256_from_v128(
      c_v128_shuffle_8(c_v256_high_v128(a), c_v256_high_v128(pattern)),
      c_v128_shuffle_8(c_v256_low_v128(a), c_v256_low_v128(pattern)));
}

// Good: illustrates input-to-output element layout transformation
// Perform a 4x4 matrix transpose going from:
// in[0] = 00 01, in[1] = 02 03, in[2] = 10 11, in[3] = 12 13...
// to:
// out[0] = 00 10, out[1] = 20 30, out[2] = 01 11, out[3] = 21 31...
static inline void transpose_arrays_s64_4x4(const int64x2_t *in,
                                            int64x2_t *out) {
  out[0] = aom_vtrn1q_s64(in[0], in[2]);
  out[1] = aom_vtrn1q_s64(in[4], in[6]);
  out[2] = aom_vtrn2q_s64(in[0], in[2]);
  out[3] = aom_vtrn2q_s64(in[4], in[6]);
}
```
