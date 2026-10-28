---
name: libaom/av1/distinguish-icdf-from-cdf-in-entropy-coding-names
description: When naming functions, parameters, or tables that compute or manipulate inverted cumulative distribution arrays (AOM_ICDF), use 'icdf' rather than 'cdf'
rubric: readability
severity: should_fix
---

**Rule.** Name functions, variables, and parameters that compute, convert, or
manipulate libaom's inverted cumulative distribution format (AOM_ICDF) using
'icdf' rather than 'cdf'. Reserve 'cdf' strictly for standard non-inverted
cumulative distribution arrays and functions.

**Why.** libaom's arithmetic and entropy coding tables store inverted CDFs
(where values equal CDF_PROB_TOP minus cumulative probability) rather than
standard CDFs. Using 'icdf' makes the underlying inverted mathematical
representation and array layout explicit, preventing confusion and misuse across
entropy coding pipelines.

## Examples

Bad:

```cpp
// Ambiguous naming: converts PDF to inverted CDF representation but named 'cdf'.
void av1_pdf2cdf(const float *pdf, aom_cdf_prob *cdf, int nsymbs);

void encode_symbol(aom_writer *w, int symb, const aom_cdf_prob *cdf);
```

Good:

```cpp
// Explicit naming: clearly indicates inverted CDF representation.
void av1_pdf2icdf(const float *pdf, aom_cdf_prob *icdf, int nsymbs);

void encode_symbol(aom_writer *w, int symb, const aom_cdf_prob *icdf);
```
