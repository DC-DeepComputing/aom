---
name: libaom/av1/consolidate-buffer-initialization-and-padding
description: When initializing scratch or neighbour buffers against uninitialized reads, consolidate filling with default fallback values or boundary padding instead of redundant full-buffer zeroing
rubric: design
severity: should_fix
---

**Rule.** When initializing scratch, border, or neighbour buffers (such as to
guard against SIMD over-reads or sanitizer/Valgrind uninitialized memory
warnings), initialize them directly with their default fallback values or
consolidate the zeroing into existing boundary padding and edge-extension logic.
Avoid adding redundant upfront full-buffer `memset` or `aom_memset16` calls when
downstream branches or fallback paths will compute, fill, or overwrite the
required pixel range.

**Why.** Adding redundant upfront full-buffer zeroing (`memset` /
`aom_memset16`) only to overwrite the buffer in downstream branches or boundary
padding incurs duplicate memory writes on performance-critical per-block hot
paths and scatters initialization logic across disjoint sections of the
function.

## Examples

Bad:

```cpp
// Variant 1: Redundant zero-init before boundary padding
memset(left_data, 0, sizeof(left_data));
const int num_left_pixels_needed = txhpx + (need_bottom ? txwpx : 3);
if (i < num_left_pixels_needed) {
  memset(&left_col[i], left_col[i - 1], num_left_pixels_needed - i);
}

// Variant 2: Zero-filling upfront and overwriting with fallback values
memset(left_data, 0, sizeof(left_data));
if (n_left_px > 0) {
  fill_left_pixels(left_data, n_left_px);
} else if (n_top_px > 0) {
  aom_memset16(left_col, above_ref[0], num_left_pixels_needed);
} else {
  aom_memset16(left_col, base + 1, num_left_pixels_needed);
}
```

Good:

```cpp
// Variant 1: Consolidated boundary padding calculation
const int num_left_pixels_needed =
    txhpx + (n_bottomleft_px >= 0 ? txwpx : 0);
if (i < num_left_pixels_needed) {
  memset(&left_col[i], left_col[i - 1], num_left_pixels_needed - i);
}

// Variant 2: Pre-fill directly with default fallback value
aom_memset16(left_data, base + 1, NUM_INTRA_NEIGHBOUR_PIXELS);
if (n_left_px > 0) {
  fill_left_pixels(left_data, n_left_px);
} else if (n_top_px > 0) {
  aom_memset16(left_col, above_ref[0], num_left_pixels_needed);
}
```
