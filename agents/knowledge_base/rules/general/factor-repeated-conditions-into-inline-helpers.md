---
name: libaom/factor-repeated-conditions-into-inline-helpers
description: When evaluating repeated conditions, parameter mappings, plane-dependent block/transform sizes, or bit-depth-independent setup across functions or pipelines, extract shared static inline helpers
rubric: design
severity: should_fix
---

**Rule.** When non-trivial conditional expressions, multi-term struct-field
derivations, tool/mode/angle applicability checks, parameter-to-filter mappings,
plane-dependent block/transform size selections, or bit-depth-independent setup
logic are evaluated across multiple call sites, parallel pipelines (such as
standard uint8_t* vs. highbd uint16_t* variants), or architecture
implementations (C vs. SIMD backends like SSE2, AVX2, or NEON), extract the
common logic into a shared static INLINE / AOM_INLINE helper function (such as
is_*(), has_*(), av1_allow_*(), get_plane_tx_size(), or derive_*_offsets()) in
an appropriate common header or file scope. Do not duplicate identical
multi-clause conditions, ternary lookups, plane/angle checks, coordinate
arithmetic, or array index lookups across individual call sites or pipeline
variants. Additionally, when a compound condition is evaluated repeatedly within
a single local scope, extract it into a descriptive named const boolean
variable.

**Why.** Duplicating multi-term conditions, mode/plane gating expressions,
coordinate arithmetic, or plane-dependent size logic across sibling functions,
bit-depth paths, and SIMD implementations creates boilerplate, obscures feature
gating, and introduces synchronization hazards where updates made in one stage
or bit-depth variant fail to be replicated in others. Centralizing invariant
definitions, tool applicability rules, filter selections, coordinate
derivations, and plane transform calculations into static inline helpers
establishes a single source of truth, improves readability through descriptive
names, and incurs zero runtime overhead.

## Examples

Bad:

```cpp
// Variant 1: Duplicating plane block/transform size lookups across call sites
const BLOCK_SIZE bsize_base =
    plane ? mbmi->chroma_ref_info.bsize_base : mbmi->sb_type;
const TX_SIZE plane_tx_size =
    plane ? av1_get_max_uv_txsize(bsize_base, pd->subsampling_x, pd->subsampling_y)
          : mbmi->inter_tx_size[av1_get_txb_size_index(plane_bsize, blk_row, blk_col)];

// Variant 2: Duplicating coordinate/offset setup between 8-bit and highbd variants
void apply_filter(const uint8_t *src, int stride, int type) {
  int offsets[2] = { (type == 0) ? -stride : -2 * stride, (type == 0) ? stride : 2 * stride };
  /* ... */
}
void apply_filter_hbd(const uint16_t *src, int stride, int type) {
  int offsets[2] = { (type == 0) ? -stride : -2 * stride, (type == 0) ? stride : 2 * stride };
  /* ... */
}

// Variant 3: Duplicating parameter mappings or mode checks across call sites
const InterpFilterParams *filter =
    (subpel_search == USE_2_TAPS)
        ? av1_get_4tap_interp_filter_params(BILINEAR)
        : av1_get_interp_filter_params_with_block_size(EIGHTTAP_REGULAR, 8);
```

Good:

```cpp
// Variant 1: Centralized plane transform size helper
static AOM_INLINE TX_SIZE get_plane_tx_size(const MB_MODE_INFO *mbmi,
                                            const struct macroblockd_plane *pd,
                                            BLOCK_SIZE plane_bsize,
                                            int plane, int blk_row, int blk_col) {
  const BLOCK_SIZE bsize_base =
      plane ? mbmi->chroma_ref_info.bsize_base : mbmi->sb_type;
  return plane ? av1_get_max_uv_txsize(bsize_base, pd->subsampling_x, pd->subsampling_y)
               : mbmi->inter_tx_size[av1_get_txb_size_index(plane_bsize, blk_row, blk_col)];
}

// Variant 2: Shared setup helper called by both bit-depth variants
static INLINE void derive_filter_offsets(int *offsets, int stride, int type) {
  offsets[0] = (type == 0) ? -stride : -2 * stride;
  offsets[1] = (type == 0) ? stride : 2 * stride;
}

// Variant 3: Shared parameter mapping helper
static INLINE const InterpFilterParams *av1_get_filter(int subpel_search) {
  return (subpel_search == USE_2_TAPS)
             ? &av1_interp_4tap[BILINEAR]
             : &av1_interp_filter_params_list[EIGHTTAP_REGULAR];
}
```
