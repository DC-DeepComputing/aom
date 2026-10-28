---
name: libaom/document-dynamic-ranges-and-sanitizer-workarounds
description: When documenting intermediate calculations, accumulators, or sanitizer workarounds in DSP and SIMD routines, specify dynamic ranges and explicitly annotate signed or unsigned bit widths
rubric: readability
severity: nit
---

**Rule.** In DSP and SIMD routines, document the dynamic range (min/max
numerical bounds and bit width) of intermediate calculations, accumulators, and
products to clarify overflow safety, explicitly annotating whether bit widths
represent signed or unsigned values (e.g., '14 (signed) bits', '32 (unsigned)
bits'). Additionally, when applying type conversions or arithmetic adjustments
(such as abs()) to silence compiler or sanitizer warnings (such as
-fsanitize=integer), document the affected downstream expression and its
signed/unsigned bounds.

**Why.** Intermediate sums, differences, and products in DSP kernels can
approach integer register capacity limits. Documenting numerical bounds and
explicitly stating whether bit widths are signed or unsigned eliminates
ambiguity about whether bit counts include sign bits, clarifies overflow safety,
and justifies type conversions or unsigned casting.

## Examples

Bad:

```cpp
// Example 1: Missing dynamic range or ambiguous bit width without signedness
const uint32_t mean = horizontal_add_s16x8(v_mean);
// (mean * mean): dynamic range 32 bits.
const uint32_t var = sse - ((mean * mean) >> (bwl + 2));

// Example 2: Vague sanitizer comment without downstream context or signedness
// abs() avoids -fsanitize=integer unsigned overflow warnings.
const uint32_t mean_abs = abs(horizontal_add_s16x8(v_mean));
```

Good:

```cpp
// Example 1: Explicit dynamic range and signed/unsigned bit width documented
// mean: dynamic range [-32640, 32640], 16 (signed) bits.
const int32_t mean = horizontal_add_s16x8(v_mean);
// (mean * mean): dynamic range [0, 1065369600], 31 (unsigned) bits.
const uint32_t var = sse - ((mean * mean) >> (bwl + 2));

// Example 2: Downstream context and signed/unsigned range documented for sanitizer workaround
// abs() avoids -fsanitize=integer unsigned overflow warnings when converting
// to unsigned for 'mean * mean'. Dynamic range [0, 65280], 16 (unsigned) bits.
const uint32_t mean_abs = abs(horizontal_add_s16x8(v_mean));
```
