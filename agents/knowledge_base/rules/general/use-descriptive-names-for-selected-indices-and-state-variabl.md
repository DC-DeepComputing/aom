---
name: libaom/use-descriptive-names-for-selected-indices-and-state-variabl
description: When saving loop search results, selected indices, or condition states, use descriptive variable names reflecting tracked semantics instead of cryptic abbreviations
rubric: readability
severity: nit
---

**Rule.** Use descriptive, self-explanatory variable names that clearly convey
the semantics or condition being tracked (such as `scene_change_gop_frame_index`
or `best_matching_mode`) when capturing loop search results, selected indices,
or state flags across a function scope. Avoid cryptic or ambiguous abbreviations
(such as `i_sel`, `idx_tmp`, or `sel_i`) that obscure what condition was
satisfied or what the stored value represents.

**Why.** Cryptic or abbreviated variable names like `i_sel` force readers to
trace back to loop bodies and conditional blocks to deduce what condition was
met. Descriptive names make the code self-documenting, clarify downstream
invariants and calculations, and prevent misunderstandings during maintenance.

## Examples

Bad:

```cpp
int i_sel = 0;
for (int i = 1; i < max_gop_length; i++) {
  if (rc->high_source_sad_lag[i] == 1 && i > 1) {
    i_sel = i;
    break;
  }
}
if (i_sel > 0) {
  max_gop_length = AOMMIN(max_gop_length, i_sel);
}
```

Good:

```cpp
int scene_change_gop_frame_index = 0;
for (int i = 1; i < max_gop_length; i++) {
  if (rc->high_source_sad_lag[i] == 1 && i > 1) {
    scene_change_gop_frame_index = i;
    break;
  }
}
if (scene_change_gop_frame_index > 0) {
  max_gop_length = AOMMIN(max_gop_length, scene_change_gop_frame_index);
}
```
