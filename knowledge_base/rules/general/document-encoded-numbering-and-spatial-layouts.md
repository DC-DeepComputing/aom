---
name: libaom/document-encoded-numbering-and-spatial-layouts
description: When defining spatial mode enums or implementing filtering and tap-offset derivation logic, document 2D sample layouts and numbering schemes using ASCII diagrams
rubric: readability
severity: nit
---

**Rule.** When defining enums, constants, or mode identifiers encoding spatial
coordinates or tap subsets, or when implementing spatial filtering, intra
prediction, or tap-offset derivation logic with 2D sampling footprints, document
the relative sample locations and numbering scheme using an ASCII diagram or
layout comment. Clarify the geometric sample configuration, coordinate frames,
or clockwise/raster ordering directly above the definitions or offset
calculations.

**Why.** Identifiers encoding numeric tap subsets and stride-based offset
arithmetic (such as -1 * stride - 1) obscure geometric sample relationships and
coordinate frames. Visual ASCII diagrams and layout comments make 2D spatial
neighborhoods immediately clear, preventing coordinate misinterpretations and
making filter logic easy to audit and maintain.

## Examples

Bad:

```cpp
// Example 1: Enums encoding spatial tap positions
typedef enum {
  ADAPT_FILTER_1_2_3,
  ADAPT_FILTER_0_1_3,
  ADAPT_FILTER_1_3_4,
  ADAPT_FILTER_0_1_2_3_4_LEFT,
  ADAPT_FILTER_1_2_3_4_TOP,
  ADAPT_FILTER_0_2_3,
  ADAPT_FILTER_1_2_4,
  ADAPT_FILTER_INTRA_MODES,
} ADAPT_FILTER_INTRA_MODE;

// Example 2: Spatial filter tap offset derivation
void derive_ccso_sample_pos(int *rec_idx, const int ccso_stride,
                            const uint8_t ext_filter_support) {
  if (ext_filter_support == 0) {
    rec_idx[0] = -1 * ccso_stride;
    rec_idx[1] = 1 * ccso_stride;
  } else if (ext_filter_support == 1) {
    rec_idx[0] = -1 * ccso_stride - 1;
    rec_idx[1] = 1 * ccso_stride + 1;
  }
}
```

Good:

```cpp
// Example 1: Enums encoding spatial tap positions
// Adaptive intra filter modes use subsets of 5 neighbor pixels
// surrounding the current pixel ('cur'), enumerated clockwise:
// -------------
// | 2 | 3 | 4 |
// -------------
// | 1 |cur|   |
// -------------
// | 0 |   |   |
// -------------
// E.g., ADAPT_FILTER_1_2_3 uses neighbor pixels 1, 2, and 3.
typedef enum {
  ADAPT_FILTER_1_2_3,
  ADAPT_FILTER_0_1_3,
  ADAPT_FILTER_1_3_4,
  ADAPT_FILTER_0_1_2_3_4_LEFT,
  ADAPT_FILTER_1_2_3_4_TOP,
  ADAPT_FILTER_0_2_3,
  ADAPT_FILTER_1_2_4,
  ADAPT_FILTER_INTRA_MODES,
} ADAPT_FILTER_INTRA_MODE;

// Example 2: Spatial filter tap offset derivation
void derive_ccso_sample_pos(int *rec_idx, const int ccso_stride,
                            const uint8_t ext_filter_support) {
  // Input sample locations for CCSO
  //         2 1 4
  // 6 o 5 o 3 o 3 o 5 o 6
  //         4 1 2
  if (ext_filter_support == 0) {
    rec_idx[0] = -1 * ccso_stride;
    rec_idx[1] = 1 * ccso_stride;
  } else if (ext_filter_support == 1) {
    rec_idx[0] = -1 * ccso_stride - 1;
    rec_idx[1] = 1 * ccso_stride + 1;
  }
}
```
