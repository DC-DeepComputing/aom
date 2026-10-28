---
name: libaom/document-non-obvious-arithmetic-offsets-and-biases
description: When implementing non-obvious arithmetic involving index strides, coordinate multipliers, buffer offsets, or bias adjustments, document their derivation and invariants
rubric: readability
severity: should_fix
---

**Rule.** When implementing arithmetic that incorporates non-obvious scaling
factors, index strides, coordinate multipliers, numeric offsets, bias
adjustments, sentinel margins, or cushion values (such as filter table subset
indexing, fractional coordinate conversions, register capacity offsets, carry
cushions, or negative counter compensations), document the origin, mathematical
derivation, or subset invariant in an explanatory comment or docstring. Ensure
callers and maintainers understand why specific multipliers or adjustments are
applied so internal state and data layout invariants are preserved.

**Why.** Opaque multipliers, subsampling steps, biased counters, and register
capacity cushions are not self-evident from raw arithmetic expressions alone.
Without explanatory comments detailing why specific scaling factors, index
mappings, offsets, or cushions are required, reviewers cannot verify code
correctness and maintainers risk introducing off-by-one errors or breaking
state-machine invariants during refactoring.

## Examples

Bad:

```cpp
// Example 1: Opaque index calculation and multiplier without explanation
static int cal_kernel_idx(double pos) {
  const double diff = pos - floor(pos);
  return (int)(diff / 0.125) * 2;
}

// Example 2: Magic constant offset and counter bias without explanation
uint8_t num_bytes_ready = (s >> 3) + 1;
c += 24 - (num_bytes_ready << 3);
```

Good:

```cpp
// Example 1: Documented filter subset indexing and fractional conversion
/*!\brief Calculate filter kernel index from fractional subpixel coordinate.
 * \return Kernel index doubled because only even kernels (0, 2, ... 14) are
 *         stored in the filter table.
 */
static int cal_kernel_idx(double pos) {
  const double diff = pos - floor(pos);
  const int idx = (int)(diff / 0.125);
  return idx * 2;
}

// Example 2: Documented counter bias and register cushion capacity
// Add 1 byte because enc->cnt counts 1 byte less (enc->cnt = -9) for carry cushion.
uint8_t num_bytes_ready = (s >> 3) + 1;
// Update "c" with non-ready bits in "low" (64-bit capacity: add 64 - 40 cushion bits).
c += 24 - (num_bytes_ready << 3);
```
