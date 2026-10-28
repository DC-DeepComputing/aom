---
name: libaom/keep-ported-code-faithful-to-source
description: When porting code from an external project, keep the implementation faithful to the stated source for clean provenance; introduce alternative formulations in separate CLs
rubric: change_scope
severity: should_fix
---

**Rule.** When porting code or algorithms from an external project (such as
libgav1 or libvpx), keep the implementation faithful to the stated source and
avoid mixing in snippets or idioms borrowed from other third-party projects
(such as dav1d). Propose any alternative formulations, optimizations, or
refactorings in a separate follow-up CL to maintain clear code provenance and
licensing boundaries.

**Why.** Mixing idioms or snippets from multiple third-party codebases in a
single porting commit obscures code provenance, complicates license and
copyright compliance tracking, and impairs code review. Keeping porting CLs
faithful to their stated source ensures traceability, while subsequent changes
can be evaluated on their own merits.

## Examples

Bad:

```cpp
// Porting update_cdf from libgav1, but mixing in dav1d's idiom:
static inline void update_cdf(aom_cdf_prob *cdf, int8_t val, int nsymbs) {
  const int count = cdf[nsymbs];
  // ...
  cdf[nsymbs] = count + (count < 32);
}
```

Good:

```cpp
// Faithful port from libgav1 preserving exact provenance:
static inline void update_cdf(aom_cdf_prob *cdf, int8_t val, int nsymbs) {
  const int count = cdf[nsymbs];
  // ...
  cdf[nsymbs] += (count < 32);
}
// Any alternative optimization or refactoring is proposed in a separate CL.
```
