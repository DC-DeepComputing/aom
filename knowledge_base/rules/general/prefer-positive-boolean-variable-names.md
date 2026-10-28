---
name: libaom/prefer-positive-boolean-variable-names
description: When naming boolean variables, flags, or struct fields, use positive affirmative names describing the enabled behavior rather than negated conditions
rubric: readability
severity: should_fix
---

**Rule.** Name boolean variables, predicate flags, condition helpers, and struct
members with positive, affirmative phrasing describing the behavior or check
they enable (e.g., is_valid, enable_feature, high_res,
force_color_check_block_level) rather than negative phrasing or negated
conditions (e.g., is_invalid, disable_feature, low_res, *_not_set, no_*,
is_not_*). Additionally, name control flags after the downstream behavioral
effect they govern rather than the upstream condition that triggered them.

**Why.** Negatively named variables and flags frequently lead to confusing
double negatives (such as !low_res, !disable_x, !no_data, or !flag_not_set) in
conditional expressions, increasing the risk of inverted-logic bugs. Naming
flags affirmatively after their behavioral effects keeps control-flow logic
clear, readable, and easy to verify across complex branching heuristics.

## Examples

Bad:

```cpp
// Variant 1: Negated predicate variable creates double negative in checks
const int low_res = cpi->common.width * cpi->common.height < 640 * 360;
if (source_sad_nonrd >= kMedSad && x->source_variance > 0 && !low_res) {
  // ...
}

// Variant 2: Negatively named struct member / flag
int fixed_partition_color_not_set;
if (!x->fixed_partition_color_not_set) {
  skip_color_check(x);
}
```

Good:

```cpp
// Variant 1: Positive predicate name avoids inverted checks
const int high_res = cpi->common.width * cpi->common.height >= 640 * 360;
if (source_sad_nonrd >= kMedSad && x->source_variance > 0 && high_res) {
  // ...
}

// Variant 2: Affirmatively named flag conveys the enabled behavior
int force_color_check_block_level;
if (!x->force_color_check_block_level) {
  skip_color_check(x);
}
```
