---
name: libaom/keep-unshared-updates-outside-critical-sections
description: When acquiring mutexes or buffer pool locks, restrict critical sections to shared resource operations and move unshared or thread-local state updates outside lock scopes
rubric: safety
severity: should_fix
---

**Rule.** Do not perform operations that mutate thread-local, unshared, or
decoder/encoder-private state inside critical sections guarded by mutexes or
buffer pool locks. Restrict locked regions strictly to accesses and
modifications of shared data structures (such as buffer pool references or
multithreading sync queues), moving all independent state updates and
calculations before lock acquisition or after lock release.

**Why.** Holding locks while executing operations that do not access shared
state unnecessarily widens critical sections and increases lock contention
across worker threads, degrading multithreading throughput without providing
synchronization safety.

## Examples

Bad:

```cpp
lock_buffer_pool(pool);
assign_frame_buffer_p(&cm->cur_frame, frame_to_show);
cm->current_frame.order_hint = frame_to_show->order_hint;
pbi->reset_decoder_state = (frame_to_show->frame_type == KEY_FRAME);
unlock_buffer_pool(pool);
```

Good:

```cpp
lock_buffer_pool(pool);
assign_frame_buffer_p(&cm->cur_frame, frame_to_show);
pbi->reset_decoder_state = (frame_to_show->frame_type == KEY_FRAME);
unlock_buffer_pool(pool);

// Perform unshared frame state updates outside the buffer pool lock.
cm->current_frame.order_hint = frame_to_show->order_hint;
```
