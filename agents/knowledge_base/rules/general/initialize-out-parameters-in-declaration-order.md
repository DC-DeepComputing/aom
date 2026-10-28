---
name: libaom/initialize-out-parameters-in-declaration-order
description: When initializing, resetting, or freeing multiple pointer arguments in helper functions, order assignments and cleanup calls in parameter declaration sequence
rubric: readability
severity: nit
---

**Rule.** When initializing, resetting, or clearing out-parameters at the start
of a function (such as assigning NULL or 0), or when releasing resources in
deallocation and cleanup helper functions (such as calling aom_free()), order
all statements to match the exact sequence in which the parameters appear in the
function signature. Do not reorder initialization or cleanup statements
arbitrarily relative to the parameter list.

**Why.** Matching statement execution order to parameter declaration order makes
function bodies easier to read, verify, and audit against the signature,
ensuring that no out-parameters or cleanup targets are inadvertently omitted or
duplicated.

## Examples

Bad:

```cpp
// 1. Initializing out-parameters out of parameter declaration order
static bool init_buffers(int width, int height, int **luma_block,
                         int **chroma_block, int **luma_buf,
                         int **chroma_buf) {
  *luma_buf = NULL;
  *chroma_buf = NULL;
  *luma_block = NULL;
  *chroma_block = NULL;
  // ...
}

// 2. Resource deallocation out of parameter declaration order
static void free_firstpass_stats_buffers(REGIONS *temp_regions,
                                         double *filt_intra_err,
                                         double *filt_coded_err,
                                         double *grad_coded) {
  aom_free(temp_regions);
  aom_free(filt_coded_err);
  aom_free(filt_intra_err);
  aom_free(grad_coded);
}
```

Good:

```cpp
// 1. Initializing out-parameters in exact parameter declaration order
static bool init_buffers(int width, int height, int **luma_block,
                         int **chroma_block, int **luma_buf,
                         int **chroma_buf) {
  *luma_block = NULL;
  *chroma_block = NULL;
  *luma_buf = NULL;
  *chroma_buf = NULL;
  // ...
}

// 2. Resource deallocation in exact parameter declaration order
static void free_firstpass_stats_buffers(REGIONS *temp_regions,
                                         double *filt_intra_err,
                                         double *filt_coded_err,
                                         double *grad_coded) {
  aom_free(temp_regions);
  aom_free(filt_intra_err);
  aom_free(filt_coded_err);
  aom_free(grad_coded);
}
```
