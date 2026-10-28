---
name: libaom/aom_dsp/match-architecture-specific-dsp-filename-conventions
description: When creating architecture-specific source files in subdirectories like x86/ or arm/, follow existing sibling conventions (<feature>_<isa>.c) and omit redundant library prefixes like aom_
rubric: readability
severity: should_fix
---

**Rule.** Name architecture-specific source and header files in architecture
subdirectories (such as `aom_dsp/x86/` or `aom_dsp/arm/`) consistently with
existing sibling implementations for other instruction sets (e.g.,
`<feature>_<isa>.c`). Do not add redundant library prefixes like `aom_` to
filenames (e.g., use `quantize_avx2.c` rather than `aom_quantize_avx2.c` to
match `quantize_sse2.c` and `quantize_neon.c`).

**Why.** Omitting redundant library prefixes from architecture-specific files
keeps file naming uniform across architecture subdirectories, simplifies CMake
lists, and matches established sibling SIMD file patterns.

## Examples

Bad:

```cpp
// Redundant library prefix on architecture-specific source file:
aom_dsp/x86/aom_quantize_avx2.c
```

Good:

```cpp
// Consistent with sibling files (quantize_sse2.c, quantize_neon.c):
aom_dsp/x86/quantize_avx2.c
```
