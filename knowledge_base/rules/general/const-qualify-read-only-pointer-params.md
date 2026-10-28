---
name: libaom/const-qualify-read-only-pointer-params
description: When declaring function parameters for read-only structs, buffers, or pointer arrays, qualify pointees as const Type * directly instead of omitting const, using Type *const, or casting
rubric: readability
severity: should_fix
---

**Rule.** When declaring or defining functions that accept pointers, buffers,
structs, or pointer arrays from which the function and its callees only read,
qualify the pointed-to type as const (e.g., const uint8_t *src, const
MACROBLOCKD *xd, const float *buf[]) directly in the parameter list. Do not
leave read-only pointer parameters non-const, qualify only the pointer itself
(Type *const ptr), or introduce redundant local pointer aliases (such as const
Type *alias = param;). Additionally, do not cast away const on arguments with
C-style casts or const_cast to satisfy callee signatures; qualify the callee
parameter instead. When passing read-only pointers to generic container APIs
taking non-const void * (such as aom_vector_push_back), cast away const at the
container call site rather than relaxing the outer function signature.

**Why.** Qualifying read-only pointees (const Type *) enforces compile-time data
immutability, clarifies interface contracts, prevents accidental mutation, and
allows callers holding const data to pass arguments without type casts. In
contrast, Type *const only prevents local pointer reassignment while leaving the
underlying buffer mutable, local const aliases create visual clutter, and
casting away const risks undefined behavior and disables compiler safety checks.

## Examples

Bad:

```cpp
// Read-only buffer and struct parameters lack const, or use Type *const:
void av1_block_yrd_idtx(MACROBLOCK *x, uint8_t *pred_buf, int pred_stride,
                        RD_STATS *this_rdc, int *skippable, BLOCK_SIZE bsize,
                        TX_SIZE tx_size);
static AOM_INLINE void prune_rect_part_using_none_pred_mode(
    MACROBLOCKD *const xd, PartitionSearchState *part_state,
    PREDICTION_MODE mode, BLOCK_SIZE bsize);

// Redundant local pointer alias instead of const-qualifying parameter directly:
bool av1_add_to_hash_map(uint32_t *pic_hash) {
  const uint32_t *src_hash = pic_hash;
  const uint32_t val = src_hash[pos];
}

// Casting away const to satisfy helper parameter signature:
void process_layer(const float *input) {
  assign_tensor(&tensor1, (float **)&input, 1);
}
```

Good:

```cpp
// Qualify read-only input buffer and struct pointees as const:
void av1_block_yrd_idtx(MACROBLOCK *x, const uint8_t *pred_buf, int pred_stride,
                        RD_STATS *this_rdc, int *skippable, BLOCK_SIZE bsize,
                        TX_SIZE tx_size);
static AOM_INLINE void prune_rect_part_using_none_pred_mode(
    const MACROBLOCKD *xd, PartitionSearchState *part_state,
    PREDICTION_MODE mode, BLOCK_SIZE bsize);

// Const-qualify parameter directly and pass without local alias or casts:
bool av1_add_to_hash_map(const uint32_t *pic_hash) {
  const uint32_t val = pic_hash[pos];
}

void process_layer(const float *input) {
  assign_tensor(&tensor1, &input, 1);
}
```
