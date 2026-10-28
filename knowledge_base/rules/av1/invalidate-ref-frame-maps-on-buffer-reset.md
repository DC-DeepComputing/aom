---
name: libaom/av1/invalidate-ref-frame-maps-on-buffer-reset
description: When resetting reference counts of pooled frame buffers, invalidate reference-holding maps like ref_frame_map to prevent dangling buffer references and use-after-free
rubric: safety
severity: should_fix
---

**Rule.** When resetting reference counts of pooled frame buffers
(`frame_bufs[i].ref_count`) or clearing the buffer pool (such as during sequence
initialization or decoder resets), invalidate all reference-holding maps (such
as `cm->ref_frame_map` and `cm->next_ref_frame_map`, setting their entries to -1
or NULL). Never clear buffer reference counts while leaving stale indices or
pointers in reference frame maps.

**Why.** Resetting buffer reference counts marks buffer pool slots as
unreferenced and available for reallocation or reuse. If reference frame maps
retain indices or pointers to those slots without being invalidated
simultaneously, subsequent decode or encode operations will access stale or
reallocated buffer slots, causing use-after-free, memory corruption, or invalid
reference frame state.

## Examples

Bad:

```cpp
static INLINE void reset_frame_fbs(AV1_COMMON *cm) {
  RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
  lock_buffer_pool(cm->buffer_pool);
  for (int i = 0; i < FRAME_BUFFERS; ++i) {
    frame_bufs[i].ref_count = i == cm->new_fb_idx ? 1 : 0;
    frame_bufs[i].cur_frame_offset = 0;
    av1_zero(frame_bufs[i].ref_frame_offset);
  }
  unlock_buffer_pool(cm->buffer_pool);
}
```

Good:

```cpp
static INLINE void reset_frame_buffers(AV1_COMMON *cm) {
  RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
  memset(&cm->ref_frame_map, -1, sizeof(cm->ref_frame_map));
  memset(&cm->next_ref_frame_map, -1, sizeof(cm->next_ref_frame_map));

  lock_buffer_pool(cm->buffer_pool);
  for (int i = 0; i < FRAME_BUFFERS; ++i) {
    frame_bufs[i].ref_count = i == cm->new_fb_idx ? 1 : 0;
    frame_bufs[i].cur_frame_offset = 0;
    av1_zero(frame_bufs[i].ref_frame_offset);
  }
  unlock_buffer_pool(cm->buffer_pool);
}
```
