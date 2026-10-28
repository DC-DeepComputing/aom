---
name: libaom/group-numbered-fields-into-multidimensional-arrays
description: When declaring numbered or index-based related variables or struct members, group them into an array or multidimensional array rather than separate numbered identifiers
rubric: design
severity: should_fix
---

**Rule.** Group sequential, numbered, or index-based related variables, table
arrays, or struct members into a single array or multidimensional array rather
than declaring multiple distinct numbered identifiers (e.g., field0, field1,
field2). Size the array dimension using a named constant (e.g., MAX_... or
NUM_...) to enable programmatic indexing and iteration.

**Why.** Declaring separate numbered variables or struct fields duplicates
definitions and prevents programmatic indexing or iteration. It forces callers
to write repetitive boilerplate code, switch statements, or unrolled branches
across data access sites, increasing code maintenance burden and the risk of
omission bugs.

## Examples

Bad:

```cpp
// Separate numbered table arrays prevent programmatic iteration:
aom_cdf_prob drl0_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)];
aom_cdf_prob drl1_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)];
aom_cdf_prob drl2_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)];

// Accessing requires branching:
aom_cdf_prob *cdf = (drl_idx == 0) ? drl0_cdf[ctx] :
                    (drl_idx == 1) ? drl1_cdf[ctx] : drl2_cdf[ctx];
```

Good:

```cpp
// Grouped into a multidimensional array with a named bound:
aom_cdf_prob drl_cdf[MAX_DRL_BITS][DRL_MODE_CONTEXTS][CDF_SIZE(2)];

// Accessing uses direct indexing:
aom_cdf_prob *cdf = drl_cdf[drl_idx][ctx];
```
