---
name: libaom/scope-temporary-allocations-to-usage-block
description: When allocating or resizing temporary, scratch, or feature buffers, conditionally guard their allocation and lifecycle within the specific mode, architecture, or configuration blocks where used
rubric: safety
severity: should_fix
---

**Rule.** When dynamic buffers (such as temporary scratch buffers, context
structures, stats caches like `sb_stats_cache`, mode-specific buffers like
`src_sad_blk_64x64`, or optional feature/tuning buffers like `saliency_map`) are
only required under specific configurations, execution modes, CPU architectures,
or bit depths, conditionally guard their allocation and tightly scope their
lifecycle within that specific conditional block:

1. **Feature & Tuning Configurations**: Do not unconditionally allocate memory buffers for optional features or tuning modes during encoder initialization; condition allocations on active configuration flags (e.g., `oxcf.tune_cfg.tuning == AOM_TUNE_*` or compile-time `CONFIG_*` guards).
2. **Architecture & Bit-Depth Paths**: In search or filtering routines, gate scratch/intermediate buffer allocations with compile-time SIMD guards (e.g., `#if HAVE_AVX2`, `HAVE_NEON`) and runtime bit-depth checks (`!use_highbitdepth`, `!highbd`) so buffers are allocated only for implementations that consume them.
3. **Local Lifecycle Scoping**: Co-locate size mismatch checks, `aom_free`, allocation (`aom_malloc`, `aom_calloc`, `aom_memalign`), and `CHECK_MEM_ERROR` / `AOM_CHECK_MEM_ERROR` directly inside the guarding conditional block where the buffer is used, rather than performing allocation or resizing checks in outer unconditional scopes or deferring deallocation past the usage block.

**Why.** Unconditionally allocating buffers or performing resizing and
allocation checks in outer scopes introduces unnecessary heap allocations,
increases peak memory footprint, causes cache pollution, and fractures buffer
lifecycle ownership on inactive encoder modes, architectures, or bit depths.
Scoping allocation, size validation, and deallocation tightly to the active
feature, SIMD path, and usage block ensures memory is only allocated when
required and prevents dangling pointers.

## Examples

Bad:

```cpp
// Bad 1: Unconditional feature allocation during init
CHECK_MEM_ERROR(cm, cpi->saliency_map,
                (uint8_t *)aom_calloc(cm->height * cm->width,
                                      sizeof(*cpi->saliency_map)));

// Bad 2: Scratch buffer allocated for SIMD/bit-depth paths that ignore it
#if HAVE_AVX2 || HAVE_NEON
if (!cpi->sf.lpf_sf.disable_wiener_filter) {
  CHECK_MEM_ERROR(cm, cpi->pick_lr_ctxt.dgd_avg,
                  (int16_t *)aom_memalign(32, buf_size));
}
#endif

// Bad 3: Allocation / size check in outer scope before verifying condition
if (cpi->src_sad_blk_64x64 != NULL && cpi->src_sad_blk_alloc_size != size) {
  aom_free(cpi->src_sad_blk_64x64);
  cpi->src_sad_blk_64x64 = NULL;
}
if (mode_condition && cpi->src_sad_blk_64x64 == NULL) {
  CHECK_MEM_ERROR(cm, cpi->src_sad_blk_64x64, aom_malloc(size));
}
```

Good:

```cpp
// Good 1: Gated by config and tuning flag
#if CONFIG_SALIENCY_MAP
if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_VMAF_SALIENCY_MAP) {
  CHECK_MEM_ERROR(cm, cpi->saliency_map,
                  (uint8_t *)aom_calloc(cm->height * cm->width,
                                        sizeof(*cpi->saliency_map)));
}
#endif

// Good 2: Gated by specific SIMD architecture and bit depth
#if HAVE_AVX2
if (!cpi->sf.lpf_sf.disable_wiener_filter && !highbd) {
  CHECK_MEM_ERROR(cm, cpi->pick_lr_ctxt.dgd_avg,
                  (int16_t *)aom_memalign(32, buf_size));
}
#endif

// Good 3: Size validation, allocation, and lifecycle scoped inside condition
if (mode_condition) {
  if (cpi->src_sad_blk_64x64 != NULL && cpi->src_sad_blk_alloc_size != size) {
    aom_free(cpi->src_sad_blk_64x64);
    cpi->src_sad_blk_64x64 = NULL;
  }
  if (cpi->src_sad_blk_64x64 == NULL) {
    CHECK_MEM_ERROR(cm, cpi->src_sad_blk_64x64, aom_malloc(size));
    cpi->src_sad_blk_alloc_size = size;
  }
}
```
