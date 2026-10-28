---
name: libaom/avoid-mixing-cleanups-into-functional-changes
description: When submitting functional, feature, or optimization changes, do not bundle unrelated cleanups, dead-code/macro deletions, assertion edits, or opportunistic refactoring; defer them to separate CLs
rubric: change_scope
severity: should_fix
---

**Rule.** Keep functional, feature, optimization, and bug-fix CLs strictly
focused on their stated objective. Do not bundle unrelated drive-by edits,
opportunistic refactorings, dead-code or unused macro deletions, gratuitous
helper extractions, re-indentation, or modifications/removals of existing
assertions in adjacent or unaffected functions. Keep existing legacy code and
assertions untouched and submit non-essential cleanups in dedicated follow-up
CLs.

**Why.** Bundling unrelated cleanups, dead-code or macro removals, opportunistic
refactorings, or assertion modifications with functional changes inflates review
diffs, obscures behavioral modifications, complicates review verification, and
pollutes git blame and bisect history when triaging regressions.

## Examples

Bad:

```cpp
// Variant 1: Deleting unrelated dead macros in an optimization CL
-#define FRAME_CONTEXTS (FRAME_BUFFERS + 1)
-#define FRAME_CONTEXT_DEFAULTS (FRAME_CONTEXTS - 1)

// Variant 2: Unrelated assertion removal or drive-by edit in adjacent code
-  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
-                            _mm_set1_epi16((short)0xffff)));
   const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);
```

Good:

```cpp
// Variant 1: Leave unrelated macros untouched; clean them up in a dedicated CL
#define FRAME_CONTEXTS (FRAME_BUFFERS + 1)
#define FRAME_CONTEXT_DEFAULTS (FRAME_CONTEXTS - 1)

// Variant 2: Preserve existing assertions and adjacent code intact
   assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                             _mm_set1_epi16((short)0xffff)));
   const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);
```
