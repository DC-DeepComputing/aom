---
name: libaom/av1/direct-simple-motion-search-for-single-reference
description: When evaluating simple motion search against a single reference frame, call av1_simple_motion_search_sse_var directly instead of passing a 1-element list to simple_motion_search_get_best_ref
rubric: readability
severity: should_fix
---

**Rule.** When performing simple motion search against a single specific
reference frame (such as evaluating solely LAST_FRAME or ALTREF_FRAME), invoke
av1_simple_motion_search_sse_var() directly. Do not construct a 1-element
reference list to call simple_motion_search_get_best_ref(), which is intended
for searching across multiple candidate references.

**Why.** simple_motion_search_get_best_ref() adds wrapper indirection and loop
overhead to select the best reference among candidates. Calling
av1_simple_motion_search_sse_var() directly for a single known reference avoids
single-element array boilerplate and makes the search parameters explicit.

## Examples

Bad:

```cpp
const int ref_list[] = { cpi->rc.is_src_frame_alt_ref ? ALTREF_FRAME
                                                       : LAST_FRAME };
const int num_refs = 1;
simple_motion_search_get_best_ref(cpi, x, sms_tree, sub_mi_row, sub_mi_col,
                                  subsize, ref_list, num_refs, use_subpixel, 0,
                                  &sse, &var);
```

Good:

```cpp
const int ref = cpi->rc.is_src_frame_alt_ref ? ALTREF_FRAME : LAST_FRAME;
av1_simple_motion_search_sse_var(cpi, x, sub_mi_row, sub_mi_col, subsize, ref,
                                 sms_tree->start_mvs[ref], /*num_planes=*/1,
                                 /*use_subpixel=*/1, &sse, &var);
```
