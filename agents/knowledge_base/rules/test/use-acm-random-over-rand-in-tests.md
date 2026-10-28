---
name: libaom/test/use-acm-random-over-rand-in-tests
description: When generating random test data or pixel values in unit tests, use libaom_test::ACMRandom or RandomVideoSource instead of standard C rand()
rubric: readability
severity: should_fix
---

**Rule.** Do not use standard C rand() or srand() to generate random pixel
values, coefficients, or test inputs in unit tests. Use libaom_test::ACMRandom
(from test/acm_random.h, e.g. Rand8(), Rand16(), Rand31()) or
libaom_test::RandomVideoSource initialized with a deterministic seed (such as
ACMRandom::DeterministicSeed()).

**Why.** Standard C rand() produces non-deterministic or platform-dependent
pseudo-random sequences across different operating systems and C runtime
libraries, leading to flaky test failures that are hard to reproduce.
libaom_test::ACMRandom and RandomVideoSource provide deterministic, reproducible
pseudo-random data across all platforms.

## Examples

Bad:

```cpp
for (int y = 0; y < 144; ++y) {
  for (int x = 0; x < 176; ++x) {
    raw->planes[AOM_PLANE_Y][y * raw->stride[AOM_PLANE_Y] + x] =
        static_cast<uint8_t>(rand() % 256);
  }
}
```

Good:

```cpp
libaom_test::ACMRandom rnd;
rnd.Reset(libaom_test::ACMRandom::DeterministicSeed());
for (int y = 0; y < 144; ++y) {
  for (int x = 0; x < 176; ++x) {
    raw->planes[AOM_PLANE_Y][y * raw->stride[AOM_PLANE_Y] + x] = rnd.Rand8();
  }
}
```
