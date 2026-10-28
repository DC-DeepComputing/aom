---
name: libaom/use-specific-names-for-error-fallback-flags
description: When tracking runtime failures or fallback conditions, name boolean flags with specific, natural failure predicates like malloc_failed rather than ambiguous completion flags or awkward compounds
rubric: readability
severity: should_fix
---

**Rule.** When defining status or fallback flags that track runtime failure
conditions (such as memory allocation failure or out-of-memory), name the
variable specifically and positively after the failure condition using natural
predicate phrases (e.g., malloc_failed, alloc_failed, or out_of_memory). Avoid
using generic or ambiguous completion flags (such as scaled or done) as well as
awkward or redundant compound names (such as oom_errored). Ensure the flag's
polarity and identifier clearly indicate the failure mode triggering the
fallback path.

**Why.** Generic completion flags obscure whether execution fell back due to an
unsupported configuration or an unexpected runtime error like allocation
failure. Furthermore, awkward or redundant compound names like 'oom_errored'
degrade readability. Using explicit, natural failure predicates makes fallback
triggers self-documenting, clarifies error-handling intent, and prevents logic
bugs during partial loop executions.

## Examples

Bad:

```cpp
// Bad: Using a generic completion flag obscures failure cause
int scaled = 0;
for (int i = 0; i < num_planes; ++i) {
  uint8_t *const temp_buffer = malloc(buf_size);
  if (temp_buffer) {
    scaled = 1;
    scale_plane(temp_buffer, ...);
    free(temp_buffer);
  }
}
if (!scaled) {
  av1_resize_and_extend_frame_c(...);
}

// Bad: Awkward or redundant compound name
int oom_errored = 0;
if (!temp_buffer) {
  oom_errored = 1;
}
```

Good:

```cpp
// Good: Clear, natural predicate naming the specific failure condition
int malloc_failed = 0;
for (int i = 0; i < num_planes; ++i) {
  uint8_t *const temp_buffer = malloc(buf_size);
  if (!temp_buffer) {
    malloc_failed = 1;
    break;
  }
  scale_plane(temp_buffer, ...);
  free(temp_buffer);
}
if (malloc_failed) {
  av1_resize_and_extend_frame_c(...);
}
```
