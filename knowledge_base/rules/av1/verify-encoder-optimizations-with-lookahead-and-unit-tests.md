---
name: libaom/av1/verify-encoder-optimizations-with-lookahead-and-unit-tests
description: When adding or modifying encoder-side optimizations or SIMD kernels, verify them with encoder unit tests and lookahead encodes rather than decoder-only suites
rubric: testing
severity: blocking
---

**Rule.** When validating encoder-side optimizations or architecture-specific
SIMD kernels (such as temporal filtering, TPL, or rate control), do not rely
solely on decoder bitstream test suites (e.g., Argon streams). Ensure changes
are verified with dedicated encoder unit tests in test/ and end-to-end encode
checksum comparisons with lookahead enabled (--lag-in-frames > 0).

**Why.** Decoder conformance test suites only exercise the decode pipeline and
never execute encode-only tools like temporal filtering. Features that depend on
lookahead frames require non-zero lag-in-frames and dedicated encoder tests to
execute and catch SIMD or algorithmic regressions.

## Examples

Bad:

```cpp
// Relying only on decoder conformance test suites (e.g., Argon streams)
// to validate encoder temporal filtering SIMD implementations.
// Note: Decoder suites do not exercise encode-side filtering routines.
```

Good:

```cpp
// 1. Verify SIMD kernels with dedicated encoder unit tests against C reference:
//    test/temporal_filter_test.cc (e.g., HBDtemporal_filter_test_neon)
// 2. Run end-to-end encode checksum comparisons with lookahead enabled:
//    aomenc --lag-in-frames=19 --passes=1 input.y4m -o output.ivf
```
