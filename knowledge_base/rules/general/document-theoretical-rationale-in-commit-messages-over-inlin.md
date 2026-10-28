---
name: libaom/document-theoretical-rationale-in-commit-messages-over-inlin
description: When writing inline comments for defensive code or sanitizer workarounds, keep inline notes concise and move deep theoretical rationale to the commit message
rubric: readability
severity: nit
---

**Rule.** Avoid adding long, esoteric inline comments explaining why defensive
code or sanitizer workarounds are theoretically redundant under two's-complement
arithmetic or compiler optimizations. Keep inline comments concise and focused
on practical invariants (such as dynamic range or bit width), and document deep
theoretical explanations, historical archaeology, or sanitizer workaround
justifications in the commit message instead.

**Why.** Inline comments detailing deep theoretical proofs of why defensive code
is redundant create visual clutter and confuse readers about what invariants
actually matter at runtime. Keeping inline comments concise focuses reader
attention on essential code invariants while preserving deep technical context
in version control history.

## Examples

Bad:

```cpp
// abs() is not strictly necessary when converting the value to unsigned for
// use in the 'mean * mean' calculation, but avoids unsigned integer overflow
// warnings when using -fsanitize=integer. Dynamic range [0, 65280], 16
// (unsigned) bits.
const uint32_t mean = abs(horizontal_add_s16x8(v_mean));
```

Good:

```cpp
// Dynamic range [0, 65280], 16 (unsigned) bits.
const uint32_t mean_abs = abs(horizontal_add_s16x8(v_mean));
```
