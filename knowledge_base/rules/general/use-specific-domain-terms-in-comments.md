---
name: libaom/use-specific-domain-terms-in-comments
description: When writing explanatory code comments, name specific domain concepts, parameters, qualified trade-offs, and motivating use cases rather than vague placeholders
rubric: readability
severity: nit
---

**Rule.** When writing explanatory or inline code comments, use specific domain
terms for parameters, data structures, and concepts (such as 'CDEF strengths',
'transform coefficients', or 'motion vectors') instead of vague placeholder
nouns like 'entries' or 'items'. Explicitly qualify ambiguous trade-off terms
such as 'efficiency' (e.g. 'compression efficiency' vs. 'runtime efficiency'),
and name concrete motivating workloads, scenarios, or media formats (such as
'layered image encoding') rather than using vague phrases like 'in use cases'.

**Why.** Generic placeholders like 'entries' or 'in use cases' obscure which
fields are operated on or under what conditions an optimization was evaluated,
while unqualified terms like 'efficiency' conflate rate-distortion gains with
execution speed. Using precise domain terms, qualified descriptors, and concrete
motivating scenarios keeps comments self-contained and clear for maintainers.

## Examples

Bad:

```cpp
// Zero out entries with low strengths (luma and/or chroma).
if (adaptive_cdef && reduce_strength) zero_low_strengths(strengths);

// Doing this increases overall efficiency while picking inter prediction.
apply_inter_bias(bias_factor);

// Tune IQ and SSIMULACRA2 are often used in use cases where keyframes can
// be encoded at a lower quality (i.e. higher QP) than inter-coded frames.
```

Good:

```cpp
// Zero out CDEF strengths with low values (luma and/or chroma).
if (adaptive_cdef && reduce_strength) zero_low_strengths(strengths);

// Apply bias to increase perceptual compression efficiency while picking inter.
apply_inter_bias(bias_factor);

// Tune IQ and SSIMULACRA2 can be used to encode layered images, where
// keyframes could be encoded at a lower or similar quality (i.e. higher
// QP) than inter-coded frames.
```
