---
name: libaom/assert-disallowed-configurations-over-silent-fallbacks
description: Assert invariants directly with assert() for disallowed parameter configurations, unsupported format modes, or impossible states rather than silently returning fallback or clamped values
rubric: correctness
severity: should_fix
---

**Rule.** When branching on configuration parameters, bit depths (`AOM_BITS_*`),
profiles, or mode combinations where certain paths represent illegal,
unsupported, or impossible states by specification or design, enforce the
invariant using an explicit `assert()`. Do not mask upstream bugs or
specification violations by defensively returning fallback, clamped, or dummy
values (such as clamped quantization lookups or default table returns).

**Why.** Silently returning a fallback or clamped value for invalid
configurations masks upstream logic errors, corrupted state, or specification
violations during development and testing. Asserting the invalid condition halts
execution immediately in debug builds at the point of failure, making the root
cause straightforward to diagnose.

## Examples

Bad:

```cpp
if (q_clamped > MAXQ_UNEXT) {
  switch (bit_depth) {
    case AOM_BITS_8:
      // Silently fall back to clamped value for disallowed 8-bit extended Q.
      return ac_qlookup_QTX[MAXQ_UNEXT];
    case AOM_BITS_10:
      return 4 * ac_qlookup_QTX[q_clamped - qindex_offset];
    case AOM_BITS_12:
      return 16 * ac_qlookup_QTX[q_clamped - qindex_offset];
    default:
      return 0;
  }
}
```

Good:

```cpp
if (q_clamped > MAXQ_UNEXT) {
  switch (bit_depth) {
    case AOM_BITS_8:
      // Extended Q is unsupported for 8-bit depth; assert invariant.
      assert(q_clamped <= MAXQ_UNEXT);
      return 0;
    case AOM_BITS_10:
      return 4 * ac_qlookup_QTX[q_clamped - qindex_offset];
    case AOM_BITS_12:
      return 16 * ac_qlookup_QTX[q_clamped - qindex_offset];
    default:
      assert(0);
      return 0;
  }
}
```
