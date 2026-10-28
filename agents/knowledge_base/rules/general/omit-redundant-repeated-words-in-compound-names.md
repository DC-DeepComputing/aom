---
name: libaom/omit-redundant-repeated-words-in-compound-names
description: When naming identifiers (variables, struct fields, macros, constants) or writing doc comments, omit redundant repeated qualifier words or repetitive prefix/suffix terms
rubric: readability
severity: nit
---

**Rule.** When naming compound identifiers—including struct fields, variables,
flags, preprocessor macros, constants, and enum values—and writing their
accompanying documentation comments, omit redundant repeated qualifier words or
repetitive descriptor terms (such as repeating 'mode', 'type', 'flag',
'detection', or repeating a prefix term in the suffix). Use concise,
non-repetitive names that clearly identify the entity without repeating terms
already established in the identifier's prefix or context.

**Why.** Repeating qualifier or descriptor terms within a single compound
identifier, macro name, or doc comment adds visual clutter and creates overly
verbose names without providing additional clarity. Keeping identifier names
concise, focused, and non-redundant improves code readability while preserving
full semantic meaning.

## Examples

Bad:

```cpp
// Redundant repeated qualifiers in struct fields and doc comments
/*!
 * Decide whether to enable screen detection mode 2 fast detection mode.
 * 0: Regular mode
 * 1: Fast detection mode
 */
int screen_detection_mode2_fast_detection_mode;

// Repeating descriptor terms in both prefix and suffix of macro names
#define VAR_BOOST_MAX_QSTEP_RATIO_BOOST 8.0
#define FAST_SEARCH_METHOD_FAST_SEARCH 2
```

Good:

```cpp
// Concise, non-repetitive struct field and doc comment
/*!
 * Decide whether to enable screen detection mode 2 fast detection.
 * 0: Regular mode
 * 1: Fast detection
 */
int screen_detection_mode2_fast_detection;

// Concise, non-redundant macro identifiers
#define VAR_BOOST_MAX_BOOST 8.0
#define FAST_SEARCH_METHOD_FAST 2
```
