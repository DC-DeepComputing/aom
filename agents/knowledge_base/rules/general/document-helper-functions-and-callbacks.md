---
name: libaom/document-helper-functions-and-callbacks
description: When defining generic helper functions or comparison callbacks, document their ordering contract and name them after operand types rather than caller-specific domains
rubric: readability
severity: should_fix
---

**Rule.** Precede internal helper functions, comparison callbacks (such as those
passed to qsort), and worker routines with a concise function comment explaining
their purpose, contract, and ordering criteria (e.g., ordering elements from
smallest to largest). For generic comparison callbacks and helpers operating on
primitive types, name them directly after the compared types (e.g.,
comp_unsigned_int, compare_int64, compare_int16) rather than adding domain-,
caller-, or feature-specific prefixes (such as variance_comp_unsigned_int), and
avoid coupling comments or identifiers to specific call sites.

**Why.** Internal helpers and comparison callbacks often accept generic void
pointers or implement non-obvious ordering logic (such as ascending vs.
descending sort order or multi-key tiebreakers). Documenting their general
contract clarifies expectations and enables reuse, whereas coupling names or
comments to specific callers creates misleading assumptions about scope and
produces unnecessary naming noise when the helper is shared or refactored.

## Examples

Bad:

```cpp
// Anti-pattern 1: Undocumented comparator callback
static int comp_unsigned_int(const void *a, const void *b) {
  const unsigned int val1 = *(const unsigned int *)a;
  const unsigned int val2 = *(const unsigned int *)b;
  return (val1 > val2) - (val1 < val2);
}

// Anti-pattern 2: Generic primitive comparator named and commented with caller domain
// Comparer used by av1_get_block_variance_boost() to order subblocks.
static int variance_comp_unsigned_int(const void *a, const void *b) {
  const unsigned int val1 = *(const unsigned int *)a;
  const unsigned int val2 = *(const unsigned int *)b;
  return (val1 > val2) - (val1 < val2);
}

// In caller:
qsort(variances, SUBBLOCKS_IN_SB, sizeof(unsigned int),
      variance_comp_unsigned_int);
```

Good:

```cpp
// Correct: Document general contract and ordering behavior, and name
// generic comparators directly after the operand type:
// Comparer used by qsort() to order an array of unsigned int from smallest to
// largest.
static int comp_unsigned_int(const void *a, const void *b) {
  const unsigned int val1 = *(const unsigned int *)a;
  const unsigned int val2 = *(const unsigned int *)b;
  return (val1 > val2) - (val1 < val2);
}

// In callers across different features:
qsort(variances, SUBBLOCKS_IN_SB, sizeof(unsigned int), comp_unsigned_int);
```
