---
name: libaom/av1/alloc-pyramid-only-for-source-or-ref-buffers
description: When allocating frame buffers via aom_alloc_frame_buffer() or aom_realloc_frame_buffer(), pass alloc_pyramid as false (or 0) for temporary copy, scratch, and decoder buffers
rubric: readability
severity: nit
---

**Rule.** When allocating or reallocating a YV12_BUFFER_CONFIG frame buffer via
aom_alloc_frame_buffer() or aom_realloc_frame_buffer(), pass alloc_pyramid as
false (or num_pyramid_levels as 0) for temporary scratch/copy buffers and all
decoder buffers. Only enable alloc_pyramid (or cpi->alloc_pyramid) for buffers
that store source or reference frames in the encoder.

**Why.** Image downsampling pyramids are used exclusively for global motion
estimation on source and reference frames in the encoder. Allocating pyramid
memory for intermediate filter buffers, temporary copy buffers, or decoder frame
buffers wastes memory and adds unnecessary allocation overhead.

## Examples

Bad:

```cpp
// Allocating a temporary scratch or copy buffer with pyramid allocation enabled:
aom_alloc_frame_buffer(&copy_buffer, aligned_width, cm->height,
                       seq_params->subsampling_x, seq_params->subsampling_y,
                       seq_params->use_highbitdepth, AOM_BORDER_IN_PIXELS,
                       byte_alignment, cpi->alloc_pyramid, 0);
```

Good:

```cpp
// Temporary copy/scratch buffers and decoder buffers pass alloc_pyramid = false (or 0):
aom_alloc_frame_buffer(&copy_buffer, aligned_width, cm->height,
                       seq_params->subsampling_x, seq_params->subsampling_y,
                       seq_params->use_highbitdepth, AOM_BORDER_IN_PIXELS,
                       byte_alignment, false, 0);
```
