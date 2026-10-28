---
name: libaom/document-multi-level-index-mappings-with-diagrams
description: When declaring multi-level index indirection arrays or mapping tables in struct headers, document each translation layer and domain-to-range mapping with ASCII flow diagrams
rubric: readability
severity: nit
---

**Rule.** When declaring multi-level index mappings, indirection arrays, or
buffer translation tables in structs or data structures, document each mapping
stage with a clear explanation and an ASCII flow diagram showing how source
domains map to target ranges at every layer. Specify the exact index ranges,
offset adjustments (such as '[i - 1]' or '[i - LAST_FRAME]'), and how
intermediate indices resolve to final buffer or object pointers.

**Why.** Multi-layer index indirection (such as mapping logical reference frame
enums to virtual buffer slot indices and then to physical buffer pool entries)
is non-obvious and highly prone to off-by-one errors or indexing mismatches.
Explicit stage-by-stage documentation and diagrams prevent incorrect lookups and
clarify data structure invariants across the codec pipeline.

## Examples

Bad:

```cpp
// For encoder, ref_frame_map[j] (where j = remapped_reference_idx[i]), maps
// remapped reference index 'j' (that is, original reference type 'i') to
// actual index of the buffer in the buffer pool cm->buffer_pool.frame_bufs.
int remapped_ref_idx[REF_FRAMES];
```

Good:

```cpp
// Two-level mapping from reference frame type to buffer in the buffer pool:
// * 'remapped_ref_idx[i - 1]' maps reference type 'i' (LAST_FRAME ... EXTREF_FRAME)
//   to a remapped slot index 'j' (0 ... REF_FRAMES - 1).
// * 'cm->ref_frame_map[j]' maps remapped index 'j' to a RefCntBuffer pointer
//   from cm->buffer_pool->frame_bufs.
//
// LAST_FRAME,                        ...,      EXTREF_FRAME
//      |                                           |
//      v                                           v
// remapped_ref_idx[LAST_FRAME - 1],  ...,  remapped_ref_idx[EXTREF_FRAME - 1]
//      |                                           |
//      v                                           v
// ref_frame_map[],                   ...,     ref_frame_map[]
int remapped_ref_idx[REF_FRAMES];
```
