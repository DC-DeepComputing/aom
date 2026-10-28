---
name: libaom/reset-freed-pointers-to-null-before-fallible-allocations
description: When freeing struct buffer members, consecutively reset pointers to NULL and capacities to zero without memsetting entire structs; never reset tracking for un-freed containers
rubric: safety
severity: should_fix
---

**Rule.** When freeing dynamically allocated struct pointer members or buffers
(using aom_free(), free(), or dedicated cleanup helpers), keep deallocation,
pointer nulling, and metadata resets grouped consecutively. Immediately reset
freed pointers, any related aliases (such as original or aligned buffer
pointers), and associated size, capacity, or count fields to NULL and 0. Do not
leave dangling pointers or positive size counts across fallible allocation calls
(such as CHECK_MEM_ERROR(), AOM_CHECK_MEM_ERROR(), aom_malloc(), or
aom_realloc()) or in cleanup helpers, and avoid clearing size counts before
freeing the pointer. When cleaning up dynamic buffers in a context or state
struct, explicitly reset only the freed pointer and size/capacity members rather
than zeroing the entire struct with memset(), preserving caller-managed context
fields, parent references, flags, and options. Additionally, do not reset
container buffer dimension or capacity tracking variables (such as
allocated_tile_cols, allocated_tile_rows, or allocated_rows) in cleanup helpers
that only free nested member resources without deallocating the container buffer
itself; manage tracking variables strictly in lockstep with the container
buffer's lifecycle. Conversely, avoid redundantly setting member pointers to
NULL immediately prior to freeing the enclosing parent struct itself.

**Why.** In libaom, allocation macros like CHECK_MEM_ERROR() and error recovery
paths invoke aom_internal_error(), triggering a longjmp to top-level teardown
handlers. If freed pointers, aliases, or allocation sizes are not immediately
reset to NULL and 0, subsequent teardown, error recovery, or re-initialization
logic will attempt to free dangling pointers or operate on stale metadata,
causing double-free crashes or use-after-free vulnerabilities. Zeroing an entire
context struct with memset() wipes out caller-initialized fields, such as parent
context references, configuration flags, or file descriptors. Furthermore,
resetting tracking variables (like allocated_tile_cols) while leaving the
container buffer allocated causes state desynchronization where subsequent
reallocation checks leak memory or fail to free active buffers. Grouping
deallocation with explicit pointer and count resets prevents desynchronization
while avoiding redundant nulling before parent struct deallocation.

## Examples

Bad:

```cpp
// Bad: Dangling pointers and stale size metadata left across fallible calls.
aom_free(cpi->consec_zero_mv);
CHECK_MEM_ERROR(cm, cpi->consec_zero_mv,
                aom_malloc(current_size * sizeof(*cpi->consec_zero_mv)));
cpi->consec_zero_mv_alloc_size = current_size;

// Bad: Clearing counts before freeing, or redundantly nulling before parent free.
pool->num_frame_bufs = 0;
aom_free(pool->frame_bufs);
aom_free(ppi->p_mt_info.tile_thr_data);
ppi->p_mt_info.tile_thr_data = NULL;
aom_free(ppi);

// Bad: Zeroing entire context struct with memset, destroying caller fields.
void obudec_free(struct ObuDecInputContext *obu_ctx) {
  if (!obu_ctx) return;
  free(obu_ctx->buffer);
  memset(obu_ctx, 0, sizeof(*obu_ctx));
}

// Bad: Resetting container tracking when only nested resources are freed.
void av1_row_mt_mem_dealloc(AV1_COMP *cpi) {
  dealloc_tile_data_sync_buffers(cpi);
  enc_row_mt->allocated_tile_cols = 0;  // cpi->tile_data is still allocated!
  enc_row_mt->allocated_tile_rows = 0;
}
```

Good:

```cpp
// Good: Consecutively reset pointers to NULL and sizes to 0 before fallible calls.
aom_free(cpi->consec_zero_mv);
cpi->consec_zero_mv = NULL;
cpi->consec_zero_mv_alloc_size = 0;
CHECK_MEM_ERROR(cm, cpi->consec_zero_mv,
                aom_malloc(current_size * sizeof(*cpi->consec_zero_mv)));
cpi->consec_zero_mv_alloc_size = current_size;

// Good: Group deallocation, NULL assignment, and count reset; free parent directly.
aom_free(pool->frame_bufs);
pool->frame_bufs = NULL;
pool->num_frame_bufs = 0;
aom_free(ppi->p_mt_info.tile_thr_data);
aom_free(ppi);

// Good: Explicitly reset only freed pointer and size members.
void obudec_free(struct ObuDecInputContext *obu_ctx) {
  if (!obu_ctx) return;
  free(obu_ctx->buffer);
  obu_ctx->buffer = NULL;
  obu_ctx->buffer_capacity = 0;
  obu_ctx->bytes_buffered = 0;
}

// Good: Keep container tracking intact until container buffer itself is freed.
void av1_row_mt_mem_dealloc(AV1_COMP *cpi) {
  dealloc_tile_data_sync_buffers(cpi);
  // Leave allocated_tile_cols and allocated_tile_rows intact
}
```
