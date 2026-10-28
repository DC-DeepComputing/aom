---
name: libaom/consistent-variable-naming-across-symmetric-branches
description: When implementing parallel or symmetric code branches (such as compound vs non-compound filtering), keep variable naming and intermediate computation steps consistent
rubric: readability
severity: nit
---

**Rule.** Keep variable naming, intermediate value assignments, and calculation
steps consistent across parallel or symmetric code branches (such as compound
vs. non-compound filtering, horizontal vs. vertical passes, or luma vs. chroma
pipelines) within the same function. Avoid introducing divergent temporary
variable names or differing step-by-step intermediate structures when branches
perform analogous transformations.

**Why.** Matching variable names and step-by-step intermediate calculation
structures across symmetric branches allows readers and reviewers to compare
parallel logic side-by-side directly, reducing cognitive load and preventing
subtle logic drift or maintenance errors.

## Examples

Bad:

```cpp
// In compound branch:
result = _mm_sub_epi32(shifted, sub);
result = _mm_sra_epi32(_mm_add_epi32(result, round_bits_const), round_bits_shift);

// In non-compound branch:
const __m128i subbed = _mm_sub_epi32(shifted, sub);
result = _mm_sra_epi16(_mm_add_epi32(subbed, bits_const), bits_shift);
```

Good:

```cpp
// In compound branch:
result = _mm_sub_epi32(shifted, sub);
result = _mm_sra_epi32(_mm_add_epi32(result, round_bits_const), round_bits_shift);

// In non-compound branch:
result = _mm_sub_epi32(shifted, sub);
result = _mm_sra_epi16(_mm_add_epi32(result, bits_const), bits_shift);
```
