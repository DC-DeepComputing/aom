---
name: libaom/assert-preconditions-and-invariants
description: Validate function preconditions, dimension bounds, step divisibility, and codec invariants using active assertions (assert/aom_assert/IMPLIES), and tighten pruned branch checks
rubric: correctness
severity: should_fix
---

**Rule.** When functions, calculations, buffer offsets, variable counts,
SIMD/DSP kernels, unrolled loops, or fast paths rely on assumed preconditions,
parameter bounds, dimension divisibility, or codec invariants, document
non-obvious assumptions and validate them with explicit active assertions
(`assert()`, `aom_assert()`, `static_assert()`, or `assert(IMPLIES(...))`). At
function or loop entry, explicitly enforce required parameter bounds and
alignments (such as valid block dimensions `w`/`h`, non-null pointers, valid
strides, or positive step sizes `dx > 0`) rather than relying on implicit caller
guarantees or leaving checks commented out. Verify divisibility before dividing
buffer offsets or stepping counters in multi-element chunks (e.g.,
`assert((offset % 2) == 0)` or `assert((h % 2) == 0)` before `h -= 2`, `h -= 4`,
or `r += 8`), and assert minimum element counts before unrolled loops with
trailing remainder loops (e.g., `assert(n_coeffs > 8)` or `assert(h >= 5)`). If
an assertion or assumption is obsolete, delete it instead of leaving
commented-out code. Additionally, when pruning handling for smaller block sizes
based on caller invariants, tighten subsequent branch conditions from loose
inequalities (e.g., `w <= 4`) to exact equality checks (e.g., `w == 4`).

**Why.** Implicit parameter assumptions and commented-out assertions provide
zero automated validation during test execution, allowing invalid inputs,
negative counts, silent arithmetic truncation, misaligned buffer offsets, or
broken caller guarantees to cause silent data corruption, buffer overruns, or
undefined behavior. In optimized SIMD, vector, and unrolled loops, decrementing
counters in multi-element chunks without verifying step divisibility or minimum
counts can miss the loop termination condition, causing counter underflow and
out-of-bounds memory accesses. Active assertions document contract invariants
for maintainers, guide static analysis tools, and catch invalid invocations
immediately during testing and debug builds with zero runtime overhead in
release builds. Furthermore, using exact equality checks after pruning smaller
sizes clarifies intent and prevents readers from assuming unhandled sizes are
supported.

## Examples

Bad:

```cpp
// Variant 1: Stepping multi-element loop or unrolling without asserting divisibility/bounds
do {
  process_two_rows(src, dst, src_stride, dst_stride);
  src += 2 * src_stride;
  dst += 2 * dst_stride;
  h -= 2;
} while (h);

// Variant 2: Subsampling offset divides constant without asserting divisibility
const int offset = BORDER_PADDING * stride / 2 + BORDER_PADDING / 2;
apply_filter(dst, src + offset, stride);

// Variant 3: Commented-out assertions and loose inequality after pruning smaller sizes
void highbd_dr_prediction_z1_avx2(uint16_t *dst, ptrdiff_t stride, int bw,
                                  int bh, const uint16_t *above, int dx) {
  // assert(dx > 0);
  if (bw <= 4) {
    // Process width 4
  }
}

// Variant 4: Fast path relies on undocumented, unasserted conditional invariant
const int32_t pu_edge = mi_prev != mbmi;
if (!pu_edge) {
  curr_skipped = mbmi->skip_txfm && is_inter_block(mbmi);
}
```

Good:

```cpp
// Variant 1: Assert dimension divisibility before stepped loop processing
assert((h % 2) == 0 && h > 0);
do {
  process_two_rows(src, dst, src_stride, dst_stride);
  src += 2 * src_stride;
  dst += 2 * dst_stride;
  h -= 2;
} while (h > 0);

// Variant 2: Assert divisibility invariant to catch unaligned dimensions
assert((BORDER_PADDING % 2) == 0);
const int offset = BORDER_PADDING * stride / 2 + BORDER_PADDING / 2;
apply_filter(dst, src + offset, stride);

// Variant 3: Active assertions for parameters and tightened exact branch check
void highbd_dr_prediction_z1_avx2(uint16_t *dst, ptrdiff_t stride, int bw,
                                  int bh, const uint16_t *above, int dx) {
  assert(dx > 0);
  aom_assert((bw == 8 || bw == 4) && (bh == 8 || bh == 4));
  if (bw == 4) {
    // Process width 4
  }
}

// Variant 4: Document assumption and assert conditional invariant with IMPLIES
const int32_t pu_edge = mi_prev != mbmi;
// Fast path assumes transform blocks within a prediction block have matching sizes.
assert(IMPLIES(!pu_edge, pv_ts == ts));
if (!pu_edge) {
  curr_skipped = mbmi->skip_txfm && is_inter_block(mbmi);
}
```
