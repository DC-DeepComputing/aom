---
name: libaom/set-feature-flags-after-resource-allocation
description: When configuring features or states requiring dynamic resource allocations, set feature flags only after all dependent allocations succeed to prevent null-pointer dereferences during cleanup
rubric: safety
severity: should_fix
---

**Rule.** Do not enable feature or state flags before required resource
allocations and validations have succeeded; set the flag only after all
dependent allocations return successfully. This ensures that failure teardown
routines guarded by feature flags do not attempt to free unallocated structures
or dereference NULL pointers.

**Why.** Cleanup and error-recovery routines (such as dealloc_compressor_data or
encoder teardown) often check feature and mode flags to determine whether
dependent buffers and contexts need deallocation. If a flag is enabled before
allocation completes and an allocation fails, the cleanup path assumes the
subsystem is initialized and dereferences NULL pointers.

## Examples

Bad:

```cpp
// Anti-pattern: Enabling flag before allocation causes teardown to free NULL state
ctx->ppi->use_svc = 1;
if (!av1_alloc_layer_context(cpi, num_layers)) {
  return AOM_CODEC_MEM_ERROR;
}
```

Good:

```cpp
// Correct: Enable flag only after allocation succeeds
if (!av1_alloc_layer_context(cpi, num_layers)) {
  return AOM_CODEC_MEM_ERROR;
}
ppi->use_svc = 1;
```
