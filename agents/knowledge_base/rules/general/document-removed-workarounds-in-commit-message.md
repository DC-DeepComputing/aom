---
name: libaom/document-removed-workarounds-in-commit-message
description: When removing or deprecating compatibility headers, utility files, or platform workarounds, ensure the commit message comprehensively lists all eliminated functionality
rubric: readability
severity: should_fix
---

**Rule.** When removing or deprecating compatibility headers, utility files, or
legacy compiler/platform workarounds, ensure the commit message comprehensively
describes all eliminated features and workarounds. Do not list only a subset of
the removed functionality in the commit description.

**Why.** Commit messages serve as the permanent historical record for why
compatibility shims were deleted and what compiler or platform requirements
changed. Omitting non-trivial workarounds or features obscures the full
compatibility impact of the change from future maintainers.

## Examples

Bad:

```cpp
Remove aom_ports/msvc.h

The aom_ports/msvc.h header provides snprintf() and round() for MSVC
older than Visual Studio 2015 and Visual Studio 2013, respectively.
```

Good:

```cpp
Remove aom_ports/msvc.h

The aom_ports/msvc.h header provides snprintf(), round(), and AVX
intrinsic workarounds for MSVC older than Visual Studio 2015 and
Visual Studio 2013, respectively.
```
