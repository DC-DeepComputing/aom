---
name: libaom/document-error-path-cleanup-in-teardown
description: When adding resource deallocations to top-level teardown functions for buffers normally freed locally, document that the cleanup handles error or early-termination paths
rubric: readability
severity: nit
---

**Rule.** When adding resource deallocations or cleanup calls (such as
dealloc_compressor_data(), free_thread_data(), or encoder_destroy()) to
top-level teardown routines for buffers normally allocated and freed within a
specific pipeline stage, document the cleanup with an explanatory comment.
Explicitly specify which allocation function created the buffers and which early
termination, error recovery, or longjmp path bypasses normal stage deallocation.

**Why.** Pipeline stage buffers (such as temporal filter data) are normally
allocated and freed locally within their respective stage processing routines.
Deallocation calls in top-level teardown functions can appear redundant, like
dead code, or like double-free bugs unless explicitly documented as a fallback
for error recovery paths that abort normal execution.

## Examples

Bad:

```cpp
// Deallocate temporal filter buffers.
if (t < num_tf_workers) {
  tf_dealloc_data(&td->tf_data, is_highbitdepth);
}
tf_dealloc_data(&cpi->td.tf_data, is_highbitdepth);
```

Good:

```cpp
// Ensure buffers allocated by tf_alloc_and_reset_data() in
// prepare_tf_workers() are freed if an error occurs during temporal
// filtering (where early exit or longjmp bypasses normal tf_dealloc_data()).
if (t < num_tf_workers) {
  tf_dealloc_data(&td->tf_data, is_highbitdepth);
}

// Ensure single-threaded encode buffers from tf_alloc_and_reset_data()
// in av1_temporal_filter() are freed if an early error bypasses stage cleanup.
tf_dealloc_data(&cpi->td.tf_data, is_highbitdepth);
```
