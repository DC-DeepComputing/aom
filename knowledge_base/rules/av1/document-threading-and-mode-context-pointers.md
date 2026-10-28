---
name: libaom/av1/document-threading-and-mode-context-pointers
description: When declaring struct fields or context pointers with threading- or mode-dependent semantics, document their purpose and when values diverge across execution modes
rubric: readability
severity: should_fix
---

**Rule.** When adding struct fields or context pointers whose values, lifetimes,
or targets vary across execution or threading modes (e.g., row-mt vs. tile-mt
vs. single-thread), document their exact purpose in a doc comment (/*! \brief
... */). Explicitly state how they differ from similar existing fields and under
which threading configurations their pointers point to temporary working copies
versus finalized contexts.

**Why.** In multi-threaded encoder pipelines, worker threads often operate on
temporary scratch buffers or row contexts that diverge from finalized tile
contexts used during bitstream packing. Undocumented or vaguely commented
context pointers make it difficult for reviewers and maintainers to determine
ownership, synchronization requirements, and buffer lifetime across threading
modes.

## Examples

Bad:

```cpp
// In struct macroblock (av1/encoder/block.h)
FRAME_CONTEXT *tile_ctx;
// Context used during bitstream pack
FRAME_CONTEXT *tile_pb_ctx;
```

Good:

```cpp
// In struct macroblock (av1/encoder/block.h)
FRAME_CONTEXT *tile_ctx;
/*! \brief Entropy context for the current tile.
 *
 * This context is used to update color_map_cdf pointer used during pack
 * bitstream. In single-thread and tile-multithreading modes, this pointer is
 * identical to xd->tile_ctx; in row-mt mode, xd->tile_ctx points to a
 * temporary context while tile_pb_ctx points to the accurate tile context.
 */
FRAME_CONTEXT *tile_pb_ctx;
```
