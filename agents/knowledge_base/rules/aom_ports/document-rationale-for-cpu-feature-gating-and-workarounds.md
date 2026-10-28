---
name: libaom/aom_ports/document-rationale-for-cpu-feature-gating-and-workarounds
description: When implementing hardware feature gating, CPUID capability checks, or microarchitectural workarounds in CPU detection code, document the technical rationale in explanatory comments
rubric: readability
severity: nit
---

**Rule.** When gating hardware instruction sets, CPUID capabilities, or SIMD
feature flags on non-standard sub-features or architectural workarounds,
document the underlying technical rationale in code comments. Explain the
microarchitectural trade-offs or constraints (such as frequency downclocking,
turbo curve penalties, or dependency on sibling extensions) that necessitate the
condition.

**Why.** Gating CPU capabilities on unexpected sub-features or complex compound
conditions appears arbitrary without context. Documenting the microarchitectural
rationale prevents future maintainers from accidentally removing or
misconfiguring necessary performance and hardware compatibility workarounds.

## Examples

Bad:

```cpp
// Only set HAS_AVX512 flag if AVX512_DL feature is supported.
if ((xgetbv() & 0xe6) == 0xe6) {
  flags |=
      FEATURE_SET(reg_ebx, AVX512) && FEATURE_SET(reg_ecx, AVX512_DL)
          ? HAS_AVX512
          : 0;
}
```

Good:

```cpp
// Only set HAS_AVX512 flag if AVX512_DL features are supported.
// Older AVX512 implementations (such as Skylake) have turbo curves that
// are currently problematic for mixed AVX512/AVX2 code.
if ((xgetbv() & 0xe6) == 0xe6) {
  flags |=
      FEATURE_SET(reg_ebx, AVX512) && FEATURE_SET(reg_ecx, AVX512_DL)
          ? HAS_AVX512
          : 0;
}
```
