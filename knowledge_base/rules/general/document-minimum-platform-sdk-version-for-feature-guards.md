---
name: libaom/document-minimum-platform-sdk-version-for-feature-guards
description: When conditionally guarding OS or SDK feature macros and APIs for backwards compatibility, document the minimum SDK or OS version in a comment
rubric: readability
severity: nit
---

**Rule.** When conditionally guarding platform, OS, or SDK feature macros and
API symbols with #if defined(...) or version checks for backward compatibility
with older build toolchains, add an explanatory comment immediately above the
guard documenting the minimum SDK, OS, or API version that introduced the
symbol.

**Why.** Explicitly documenting the minimum SDK or OS version required for each
feature macro or API clarifies why conditional guards exist and enables future
maintainers to determine when toolchain baseline requirements have advanced
enough to safely remove legacy fallbacks.

## Examples

Bad:

```cpp
#if HAVE_SVE
#if defined(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE)
  if (IsProcessorFeaturePresent(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE)) {
    flags |= HAS_SVE;
  }
#endif  // defined(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE)
#endif  // HAVE_SVE
```

Good:

```cpp
#if HAVE_SVE
// Support for PF_ARM_SVE_INSTRUCTIONS_AVAILABLE was added in Windows SDK 26100.
#if defined(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE)
  if (IsProcessorFeaturePresent(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE)) {
    flags |= HAS_SVE;
  }
#endif  // defined(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE)
#endif  // HAVE_SVE
```
