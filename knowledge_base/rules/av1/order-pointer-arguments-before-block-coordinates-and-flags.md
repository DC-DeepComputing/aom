---
name: libaom/av1/order-pointer-arguments-before-block-coordinates-and-flags
description: When defining AV1 helper functions, place context pointers (AV1_COMP before MACROBLOCK) and buffer pointers before spatial coordinates (mi_row, mi_col) and trailing flags
rubric: readability
severity: nit
---

**Rule.** When defining or declaring AV1 helper and encoder functions, follow
canonical parameter ordering: place high-level context pointers (AV1_COMP *cpi)
before macroblock context (MACROBLOCK *x / MACROBLOCKD *xd), place all context,
buffer, and scale pointer parameters before spatial coordinates (mi_row, mi_col,
bsize), and place search methods or boolean flags last. Do not invert context
hierarchies or append pointer arguments after coordinate and flag parameters.

**Why.** AV1 function signatures adhere to a hierarchical parameter ordering
convention (encoder context -> macroblock context -> data/scale pointers ->
spatial coordinates -> flags/options). Maintaining this ordering avoids
confusion at call sites, prevents misplaced arguments, and ensures consistent
API design across encoder and decoder modules.

## Examples

Bad:

```cpp
// Bad: MACROBLOCK parameter placed before AV1_COMP parameter.
static AOM_INLINE const search_site_config *av1_get_search_site_config(
    MACROBLOCK *x, const AV1_COMP *cpi, SEARCH_METHODS search_method);

// Bad: Pointer parameter (sf_no_scale) appended after coordinates and flags.
static void setup_planes(AV1_COMP *cpi, MACROBLOCK *x, unsigned int *y_sad,
                         unsigned int *y_sad_g, unsigned int *y_sad_alt,
                         unsigned int *y_sad_last,
                         MV_REFERENCE_FRAME *ref_frame_partition, int mi_row,
                         int mi_col, bool is_small_sb, bool scaled_ref_last,
                         struct scale_factors *sf_no_scale);
```

Good:

```cpp
// Good: AV1_COMP parameter placed before MACROBLOCK parameter.
static AOM_INLINE const search_site_config *av1_get_search_site_config(
    const AV1_COMP *cpi, MACROBLOCK *x, SEARCH_METHODS search_method);

// Good: All pointer parameters placed before coordinates and flags.
static void setup_planes(AV1_COMP *cpi, MACROBLOCK *x, unsigned int *y_sad,
                         unsigned int *y_sad_g, unsigned int *y_sad_alt,
                         unsigned int *y_sad_last,
                         MV_REFERENCE_FRAME *ref_frame_partition,
                         struct scale_factors *sf_no_scale, int mi_row,
                         int mi_col, bool is_small_sb, bool scaled_ref_last);
```
