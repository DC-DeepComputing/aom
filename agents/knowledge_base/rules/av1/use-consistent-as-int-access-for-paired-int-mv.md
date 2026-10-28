---
name: libaom/av1/use-consistent-as-int-access-for-paired-int-mv
description: When assigning or copying paired int_mv union fields (such as this_mv and comp_mv), use consistent member access syntax (such as .as_int) across both
rubric: readability
severity: nit
---

**Rule.** When assigning, copying, or comparing paired or adjacent int_mv union
members (such as this_mv and comp_mv in CANDIDATE_MV, or reference motion vector
arrays), use consistent member access syntax across all paired fields (e.g.,
accessing .as_int on both or assigning whole unions on both). Do not mix
whole-union assignment and scalar .as_int member assignment across related
motion vectors.

**Why.** Mixing whole-union struct assignment (a = b) with scalar integer field
assignment (a.as_int = b.as_int) across paired or adjacent motion vector members
is inconsistent, obscures whether 32-bit scalar copying is intended, and harms
readability.

## Examples

Bad:

```cpp
ref_mv_stack[*refmv_count].this_mv = gm_mv_candidates[0];
ref_mv_stack[*refmv_count].comp_mv.as_int = gm_mv_candidates[1].as_int;
```

Good:

```cpp
ref_mv_stack[*refmv_count].this_mv.as_int = gm_mv_candidates[0].as_int;
ref_mv_stack[*refmv_count].comp_mv.as_int = gm_mv_candidates[1].as_int;
```
