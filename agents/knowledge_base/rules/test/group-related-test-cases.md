---
name: libaom/test/group-related-test-cases
description: When adding unit tests in test files, group related test cases together by feature or configuration rather than inserting unrelated tests into existing test clusters
rubric: readability
severity: nit
---

**Rule.** Group related test cases (such as tests covering the same mode,
feature, or configuration) together in test files. When adding new tests, place
them alongside tests exercising the same functionality rather than inserting
them in the middle of an unrelated cluster of test cases or inside unrelated
conditional preprocessor blocks.

**Why.** Keeping related test cases clustered together improves test suite
organization and maintainability. Inserting unrelated tests into an existing
group scatters related test coverage and risks placing tests inside inapplicable
preprocessor blocks (such as #if !CONFIG_REALTIME_ONLY).

## Examples

Bad:

```cpp
#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, AllIntraMode) { ... }

// Unrelated PSNR test inserted in the middle of intra mode tests
TEST(EncodeAPI, PerFramePsnr) { ... }

TEST(EncodeAPI, AllIntraAndUsePsnr) { ... }
#endif  // !CONFIG_REALTIME_ONLY
```

Good:

```cpp
#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, AllIntraMode) { ... }
TEST(EncodeAPI, AllIntraAndUsePsnr) { ... }
#endif  // !CONFIG_REALTIME_ONLY

// PSNR tests grouped separately with related PSNR test cases
TEST(EncodeAPI, PerFramePsnr) { ... }
```
