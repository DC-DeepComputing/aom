---
name: libaom/macro-names-separate-components-with-underscores
description: When defining uppercase preprocessor macros with multiple words, acronyms, or dimension descriptors, separate logical components with underscores
rubric: readability
severity: nit
---

**Rule.** When defining uppercase preprocessor macros composed of multiple
distinct words, abbreviations, acronyms, or dimension descriptors (such as block
sizes like `MXN`, `WXH`, `4D`, or SIMD targets like `NEON`), separate
constituent components with underscores in `SCREAMING_SNAKE_CASE` (e.g.,
`SAD_MXNX4D`, `VARIANCE_NXM`, `SAD_WXH_NEON`). Do not concatenate multi-token
words or size specifiers into unbroken uppercase blocks (e.g., `SADMXNX4D`,
`VARIANCENXM`, `SADWXHNEON`).

**Why.** Concatenating multiple acronyms, dimensions, or words without
underscores creates ambiguous, unreadable all-caps identifiers that blur word
boundaries and are difficult to visually parse. Using underscores maintains
consistent `SCREAMING_SNAKE_CASE` naming across all codec and DSP subsystems.

## Examples

Bad:

```cpp
// Concatenating abbreviations, words, and dimension specifiers
#define SADMXNX4D(m, n) \
  void aom_sad##m##x##n##x4d_c(...)

#define VARIANCENXM(n, m, shift) \
  variance_nxm_neon(src, src_stride, ref, ref_stride, n, m, shift)

#define VARIANCENXM_WIDE(w, h) variance_wide_neon(src, ref, w, h)
```

Good:

```cpp
// Distinct logical components separated by underscores (SCREAMING_SNAKE_CASE)
#define SAD_MXNX4D(m, n) \
  void aom_sad##m##x##n##x4d_c(...)

#define VARIANCE_NXM(n, m, shift) \
  variance_nxm_neon(src, src_stride, ref, ref_stride, n, m, shift)

#define VARIANCE_NXM_WIDE(w, h) variance_wide_neon(src, ref, w, h)
```
