---
name: libaom/av1/negate-allow-palette-in-early-exit-guards
description: When guarding palette mode evaluation or search with early-exit checks, negate the av1_allow_palette() predicate to bail out when palette coding is disallowed
rubric: correctness
severity: should_fix
---

**Rule.** When adding early-exit or bailout guards before palette search or
evaluation, verify that av1_allow_palette(...) is negated (i.e. if
(!av1_allow_palette(...)) return false;). Do not check positive
av1_allow_palette(...) in early return branches, as the helper returns non-zero
when palette mode is valid for the current block size and screen content tools.

**Why.** av1_allow_palette() evaluates to true when palette coding is permitted
for the given block size and tool settings. Omitting the negation in an early
return inverts the guard logic, causing the encoder to prematurely abort palette
search when allowed and evaluate palette coding when disallowed.

## Examples

Bad:

```cpp
// Early exit incorrectly skips palette evaluation when allowed
if (av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                      bsize)) {
  return false;
}
```

Good:

```cpp
// Early exit correctly skips palette evaluation when disallowed
if (!av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                       bsize)) {
  return false;
}
```
