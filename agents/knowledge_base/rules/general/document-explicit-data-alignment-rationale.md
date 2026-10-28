---
name: libaom/document-explicit-data-alignment-rationale
description: When specifying explicit alignment on data tables, arrays, or buffers using DECLARE_ALIGNED or alignas, document the alignment rationale and SIMD requirements in comments
rubric: readability
severity: nit
---

**Rule.** When declaring arrays, lookup tables, or scratch buffers with explicit
memory alignment constraints (such as `DECLARE_ALIGNED` or `alignas`), document
the rationale for the alignment and why the specific boundary was chosen in an
accompanying comment. State whether the alignment satisfies SIMD instruction
requirements (e.g., NEON, SSE, AVX), type-punned pointer casts (e.g., casting
byte buffers to `uint32_t*`), or memory access optimizations.

**Why.** Alignment constraints on data buffers or lookup tables often exist to
prevent memory alignment faults, undefined behavior from type-punned pointer
loads, or SIMD performance regressions. Documenting the rationale ensures future
maintainers understand why the alignment is required and do not inadvertently
reduce or remove it.

## Examples

Bad:

```cpp
DECLARE_ALIGNED(8, static const uint8_t, smooth_weights[]) = {
  // bs = 4
  255, 149, 85, 64,
  ...
};
```

Good:

```cpp
// Aligned to 8 bytes to ensure NEON loads using a cast to uint32_t* have
// sufficient alignment and to preserve alignment hints in load_weight_w8().
DECLARE_ALIGNED(8, static const uint8_t, smooth_weights[]) = {
  // bs = 4
  255, 149, 85, 64,
  ...
};
```
