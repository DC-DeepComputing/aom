---
name: libaom/test/test-intermediate-boundary-values-for-bitmask-enums
description: When testing input validation for bitmask-based or non-contiguous enums, test boundary and off-by-one values adjacent to intermediate flag combinations, not just outer endpoints
rubric: testing
severity: nit
---

**Rule.** When writing unit tests to validate input handling for bitmask-based,
flag-combined, or non-contiguous enums, test boundary and off-by-one values
adjacent to intermediate flag combinations and internal gaps in addition to
global min/max endpoints. Ensure test matrices exercise off-by-one values below
and above intermediate valid combinations to verify that validation logic
inspects full enum definitions rather than outer range bounds or partial
bitmasks.

**Why.** Bitmask-based or sparse enums have non-contiguous integer values with
valid bit patterns separated by invalid gaps. Testing only global endpoints
(such as NONE - 1 or MAX + 1) fails to detect validation bugs where intermediate
invalid values (like valid flag bits paired with an out-of-range subfield index)
are mistakenly accepted.

## Examples

Bad:

```cpp
// Missing interior boundary checks between valid bitmask format values.
static constexpr std::array<int, 4> kImageFormats = {
  AOM_IMG_FMT_NONE, AOM_IMG_FMT_NONE - 1, AOM_IMG_FMT_NV12 + 1,
  AOM_IMG_FMT_I44416 + 1
};
for (const auto img_fmt : kImageFormats) {
  EXPECT_EQ(aom_img_alloc(nullptr, static_cast<aom_img_fmt_t>(img_fmt), 64, 64, 1), nullptr);
}
```

Good:

```cpp
// Exercises both outer endpoints and intermediate boundary gaps.
static constexpr std::array<int, 5> kImageFormats = {
  AOM_IMG_FMT_NONE, AOM_IMG_FMT_NONE - 1, AOM_IMG_FMT_NV12 + 1,
  AOM_IMG_FMT_YV1216 - 1, AOM_IMG_FMT_I44416 + 1
};
for (const auto img_fmt : kImageFormats) {
  EXPECT_EQ(aom_img_alloc(nullptr, static_cast<aom_img_fmt_t>(img_fmt), 64, 64, 1), nullptr);
}
```
