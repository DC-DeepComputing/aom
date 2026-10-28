---
name: libaom/av1/break-multiplane-loop-on-failure-for-fallback
description: When processing multi-plane image buffers in a loop with fallback routines, break immediately on allocation failure and record failure with a dedicated flag
rubric: correctness
severity: should_fix
---

**Rule.** When processing multi-plane or batch image buffers in a loop where
failure (such as dynamic memory allocation failure for temporary buffers)
triggers a fallback or reference routine, immediately break out of the loop upon
error and record the failure with a dedicated flag (e.g., `malloc_failed = 1;
break;`). Do not continue subsequent loop iterations or update a mutable
per-plane success flag across iterations without breaking.

**Why.** Updating a per-plane success flag across loop iterations without an
immediate break allows subsequent successful iterations to overwrite earlier
failure status, causing the fallback routine to be skipped and leaving planes in
an inconsistent, partially processed state. Furthermore, continuing loop
iterations after an unrecoverable failure wastes CPU cycles on partial
computation that will be discarded by the fallback path.

## Examples

Bad:

```cpp
int scaled = 0;
for (int i = 0; i < num_planes; ++i) {
  uint8_t *buf = (uint8_t *)aom_malloc(size);
  if (buf) {
    scale_plane(src->buffers[i], dst->buffers[i], buf);
    scaled = 1;
    aom_free(buf);
  } else {
    scaled = 0;
  }
}
if (!scaled) fallback_scale(src, dst, num_planes);
```

Good:

```cpp
int malloc_failed = 0;
for (int i = 0; i < num_planes; ++i) {
  uint8_t *buf = (uint8_t *)aom_malloc(size);
  if (!buf) {
    malloc_failed = 1;
    break;
  }
  scale_plane(src->buffers[i], dst->buffers[i], buf);
  aom_free(buf);
}
if (malloc_failed) fallback_scale(src, dst, num_planes);
```
