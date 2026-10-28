---
name: libaom/exclude-clang-when-guarding-msvc-workarounds
description: When writing preprocessor guards for MSVC-specific pragmas or compiler bug workarounds, check !defined(__clang__) alongside defined(_MSC_VER) to avoid penalizing clang-cl
rubric: correctness
severity: should_fix
---

**Rule.** When writing preprocessor guards for MSVC-specific workarounds (such
as `#pragma optimize("", off)` or MSVC bug workarounds), verify whether the
issue applies exclusively to MSVC (cl.exe). If clang-cl is unaffected, check
`!defined(__clang__)` in addition to `defined(_MSC_VER)` (e.g., `#if
defined(_MSC_VER) && !defined(__clang__)`) rather than using `#ifdef _MSC_VER`
alone.

**Why.** On Windows, Clang in MSVC-compatibility mode (`clang-cl`) defines
`_MSC_VER` for header compatibility. Using `#ifdef _MSC_VER` alone inadvertently
applies MSVC-only workarounds—such as disabling optimizations or bypassing
standard code paths—to clang-cl builds as well.

## Examples

Bad:

```cpp
// Inadvertently disables optimizations in clang-cl as well as MSVC
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
```

Good:

```cpp
// Excludes clang-cl from the MSVC-specific optimizer workaround
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("", off)
#endif
```
