---
name: libaom/split-distinct-modes-into-dedicated-functions
description: When implementing distinct processing modes, inverse algorithms, or specialized execution paths, define dedicated separate functions rather than branching on flags inside functions or hot loops
rubric: safety
severity: should_fix
---

**Rule.** Split functions that implement distinct operations, algorithms, or
execution modes (such as convolution vs. deconvolution, forward vs. inverse
transforms, encoder vs. decoder passes, or general vs. realtime/optimized paths)
into separate, dedicated functions. Do not multiplex distinct processing
pipelines or add runtime condition checks inside performance-critical inner
loops to deduplicate code.

**Why.** Combining fundamentally distinct processing algorithms or adding
conditional branches inside hot inner loops causes code bloat, introduces branch
prediction overhead, complicates control flow, and prevents compiler inlining
and architecture-specific SIMD vectorization (such as dual filtering). Dedicated
functions clarify caller contracts, allow specific precondition assertions, and
enable modular performance optimization.

## Examples

Bad:

```cpp
// Anti-pattern 1: Multiplexing distinct operational modes via top-level conditionals
void av1_cnn_convolve_c(const float **input, int in_width, int in_height,
                        int in_stride, const CNN_LAYER_CONFIG *layer_config,
                        float **output, int out_stride) {
  if (!layer_config->deconvolve) {
    // Forward convolution processing logic...
  } else {
    // Transpose deconvolution processing logic...
  }
}

// Anti-pattern 2: Branching inside hot inner loops to deduplicate general/realtime paths
void av1_filter_block_plane_horz(..., int is_realtime) {
  for (int x = 0; x < x_range; ) {
    if (is_realtime) {
      aom_lpf_horizontal_4(p, dst_stride, ...);
      aom_lpf_horizontal_4(p + MI_SIZE, dst_stride, ...);
      x += 2;
    } else {
      aom_lpf_horizontal_4(p, dst_stride, ...);
      x += 1;
    }
  }
}
```

Good:

```cpp
// Example 1: Separate dedicated functions for distinct processing modes
void av1_cnn_convolve_c(const float **input, int in_width, int in_height,
                        int in_stride, const CNN_LAYER_CONFIG *layer_config,
                        float **output, int out_stride) {
  assert(!layer_config->deconvolve);
  // Forward convolution processing logic...
}

void av1_cnn_deconvolve_c(const float **input, int in_width, int in_height,
                          int in_stride, const CNN_LAYER_CONFIG *layer_config,
                          float **output, int out_stride) {
  assert(layer_config->deconvolve);
  // Transpose deconvolution processing logic...
}

// Example 2: Dedicated specialized function without inner-loop branching
void av1_filter_block_plane_horz_rt(...) {
  for (int x = 0; x < x_range; x += advance_units) {
    aom_lpf_horizontal_4_dual(p, dst_stride, ...);
  }
}
```
