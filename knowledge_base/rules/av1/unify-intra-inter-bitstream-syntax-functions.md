---
name: libaom/av1/unify-intra-inter-bitstream-syntax-functions
description: When reading or writing bitstream syntax sharing identical structures across intra and inter modes, unify them into a single function with conditional CDF selection
rubric: design
severity: should_fix
---

**Rule.** Unify bitstream reading and writing functions across intra and inter
modes into a single function when they share the same syntax structure and
parsing tree. Select context models, symbol distributions, and CDF tables
conditionally (e.g., branching on is_inter_block(mbmi)) within the shared
function rather than maintaining separate intra and inter parsing or
serialization functions.

**Why.** Bitstream parsing and serialization routines for transform partitioning
and syntax elements share identical syntax trees between intra and inter blocks,
differing primarily in CDF selection and context modeling. Duplicating tree
traversal across separate intra and inter routines creates redundant code and
increases the risk of encoder/decoder divergence when syntax changes.

## Examples

Bad:

```cpp
static void read_tx_partition(MACROBLOCKD *xd, MB_MODE_INFO *mbmi, ...) {
  // Parsing tree using inter CDFs
  aom_cdf_prob *cdf = ec_ctx->inter_4way_txfm_partition_cdf[is_rect][ctx];
  is_split = aom_read_symbol(r, cdf, 4, ACCT_STR);
}

static TX_SIZE read_tx_partition_intra(MACROBLOCKD *xd, MB_MODE_INFO *mbmi, ...) {
  // Duplicated parsing tree using intra CDFs
  aom_cdf_prob *cdf = ec_ctx->intra_4way_txfm_partition_cdf[is_rect][ctx];
  is_split = aom_read_symbol(r, cdf, 4, ACCT_STR);
}
```

Good:

```cpp
static TX_SIZE read_tx_partition(MACROBLOCKD *xd, MB_MODE_INFO *mbmi,
                                 TX_SIZE max_tx_size, int blk_row, int blk_col,
                                 aom_reader *r) {
  const int is_inter = is_inter_block(mbmi);
  aom_cdf_prob *cdf =
      is_inter ? ec_ctx->inter_4way_txfm_partition_cdf[is_rect][ctx]
               : ec_ctx->intra_4way_txfm_partition_cdf[is_rect][ctx];
  const int is_split = aom_read_symbol(r, cdf, 4, ACCT_STR);
  // Shared syntax tree traversal
  ...
}
```
