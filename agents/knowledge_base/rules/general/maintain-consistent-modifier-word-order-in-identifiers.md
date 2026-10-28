---
name: libaom/maintain-consistent-modifier-word-order-in-identifiers
description: When naming identifiers modifying compound domain terms or base fields, preserve natural word order rather than transposing constituent words
rubric: readability
severity: nit
---

**Rule.** When naming variables, struct members, parameters, functions, or
macros that derive from an established identifier (such as `base_qindex`) or
modify standard compound domain terms (such as `screen_content`, `rate_control`,
or `reference_frame`), preserve the underlying term's natural word order. Place
descriptive qualifiers and modifiers as consistent prefixes (e.g.,
`current_base_qindex`, `delta_base_qindex`, `high_motion_screen_content`,
`external_rate_control`) rather than transposing, splitting, or inverting
constituent words (e.g., avoid `current_qindex_base`, `qindex_base_prev`,
`high_motion_content_screen`, or `external_control_rate`).

**Why.** Preserving natural English phrasing and consistent modifier order
across related structs, local variables, parameters, and domain concepts
prevents semantic ambiguity and confusion. It also ensures code searches and
grep queries for core domain phrases (such as `screen_content` or
`rate_control`) and base identifier families (such as `base_qindex`) reliably
locate all qualified variants across the codebase.

## Examples

Bad:

```cpp
// Transposed modifier order on existing identifier (quant_params.base_qindex)
struct Macroblockd {
  int current_qindex_base;
};
int compute_delta(int qindex_base_prev);

// Scrambled compound domain terms ('content_screen' instead of 'screen_content')
int high_motion_content_screen;
int external_control_rate;
```

Good:

```cpp
// Consistent modifier word order matching base identifier (base_qindex)
struct Macroblockd {
  int current_base_qindex;
};
int compute_delta(int prev_base_qindex);

// Natural English word order preserving compound domain terms
int high_motion_screen_content;
int external_rate_control;
```
