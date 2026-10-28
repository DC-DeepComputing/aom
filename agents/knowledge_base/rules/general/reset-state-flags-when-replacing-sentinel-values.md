---
name: libaom/reset-state-flags-when-replacing-sentinel-values
description: When replacing sentinel values with explicit boolean state flags for initialization tracking, reset the flags on all reconfiguration, teardown, and buffer reinitialization paths
rubric: correctness
severity: should_fix
---

**Rule.** When replacing sentinel values (such as zero dimensions or NULL
pointers used to track initialization state) with explicit boolean state flags
(e.g., *_setup_done or *_initialized), verify that every code path that
previously modified or cleared the sentinel to trigger reinitialization also
resets the boolean flag. In particular, ensure reconfiguration, resolution
updates, and buffer reallocation routines explicitly set the flag to false when
teardown or re-setup is required.

**Why.** If reconfiguration or resolution change paths update cached dimension
fields or reallocate buffers without resetting the explicit setup flag,
subsequent passes will incorrectly assume the state is already initialized and
skip necessary setup routines, causing stale configurations or runtime crashes.

## Examples

Bad:

```cpp
// Updating resolution or reconfiguring buffers without resetting the setup flag
// leaves state marked as initialized, skipping required setup steps.
initial_dimensions->width = cm->width;
initial_dimensions->height = cm->height;
```

Good:

```cpp
// Explicitly reset the setup tracking flag along with dimension updates.
initial_dimensions->width = cm->width;
initial_dimensions->height = cm->height;
cpi->frame_size_related_setup_done = false;
```
