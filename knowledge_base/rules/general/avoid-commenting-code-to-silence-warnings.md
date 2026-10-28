---
name: libaom/avoid-commenting-code-to-silence-warnings
description: When branching on hardcoded local constants or addressing unreachable code warnings, parameterize the function from callers or delete dead branches rather than commenting out code
rubric: readability
severity: should_fix
---

**Rule.** Do not branch on hardcoded local constant variables or comment out
active checks to silence compiler and static analysis warnings about unreachable
code. If multi-case logic or alternative branches must be preserved for future
or conditional use, pass the setting as a function parameter from callers
(documenting call sites with inline parameter comments like /*param=*/value);
otherwise, delete the dead code branches and simplify the function.

**Why.** Hardcoding local constants and branching on them creates unreachable
code paths that trigger compiler and static analysis defects (such as Coverity
DEADCODE warnings), while commenting out checks obscures logic and leaves dead
code. Parameterizing the function from callers keeps the implementation general
and verifiable, while deleting unneeded branches removes clutter.

## Examples

Bad:

```cpp
// Variant 1: Branching on a hardcoded local constant creates unreachable dead code
static void dynamic_resize_one_pass_cbr(AV1_COMP *cpi) {
  int one_half_only = 1;
  ...
  resize_action = one_half_only ? DOWN_ONEHALF : DOWN_THREEFOUR;
}

// Variant 2: Commenting out checks to silence unreachable branch warnings
static void get_variance_stats(const MACROBLOCK *x, ...) {
  const int num_planes = 1;
  for (int plane = 0; plane < num_planes; ++plane) {
    // if (plane && !xd->is_chroma_ref) break;
    ...
```

Good:

```cpp
// Variant 1: Parameterize and annotate call sites
static void dynamic_resize_one_pass_cbr(AV1_COMP *cpi, int one_half_only) {
  ...
  resize_action = one_half_only ? DOWN_ONEHALF : DOWN_THREEFOUR;
}
// Caller:
dynamic_resize_one_pass_cbr(cpi, /*one_half_only=*/1);

// Variant 2: Pass parameter instead of commenting out checks
static void get_variance_stats(const MACROBLOCK *x, ..., int num_planes) {
  for (int plane = 0; plane < num_planes; ++plane) {
    if (plane && !xd->is_chroma_ref) break;
    ...
  }
}
// Caller:
get_variance_stats(x, ..., /*num_planes=*/1);
```
