---
name: libaom/test/document-probabilistic-test-parameter-sampling-intent
description: When probabilistically selecting test parameters or branching on random generator values, add a comment documenting the intended sampling frequency and test rationale
rubric: readability
severity: nit
---

**Rule.** When selecting test parameters or inputs probabilistically using
random number generators (e.g. in ternary expressions like `rnd.Rand8() < 16 ?
rnd.Rand8() : default` or conditional branches), add an explanatory comment
specifying the intended sampling frequency or percentage and the testing
rationale. Clarify why values are sampled conditionally and what edge cases or
boundary conditions the randomized path exercises.

**Why.** Repeated calls to random number generators in conditional or ternary
expressions can appear ambiguous, accidental, or like uncached duplicate
evaluations to reviewers. Explicit comments stating the probability (e.g. ~6% of
the time) and testing intent clarify the test's execution profile and prevent
confusion during review and maintenance.

## Examples

Bad:

```cpp
// Periodically test filters that extend out of bounds.
const int col = rnd_.Rand8() < 16 ? rnd_.Rand8() : 32;
const int row = rnd_.Rand8() < 16 ? rnd_.Rand8() : 32;
```

Good:

```cpp
// Around 6% of the time (16/256), use a random column/row to test
// out-of-bounds filtering cases; otherwise use the standard border offset.
const int col = rnd_.Rand8() < 16 ? rnd_.Rand8() : 32;
const int row = rnd_.Rand8() < 16 ? rnd_.Rand8() : 32;
```
