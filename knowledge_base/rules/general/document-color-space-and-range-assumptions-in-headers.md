---
name: libaom/document-color-space-and-range-assumptions-in-headers
description: When declaring color space or format conversion functions in headers, document implicit assumptions such as matrix coefficients, sample ranges, and intermediate spaces
rubric: readability
severity: nit
---

**Rule.** When declaring color space, transfer, or format conversion functions
in header files, document all implicit assumptions in a preceding comment block.
Explicitly specify the expected matrix coefficients (such as BT.709 or BT.601),
sample range (limited vs. full range), transfer characteristics, and any
intermediate color spaces (such as sRGB) used during conversion.

**Why.** Color space and format conversion routines frequently rely on fixed
matrix coefficients and range conventions that are not encoded in C types or
buffer descriptors. Documenting these implicit assumptions in header
declarations prevents callers from passing mismatched inputs, avoiding color
distortion, clipping, or incorrect metric calculations.

## Examples

Bad:

```cpp
void aom_yuv_to_xyb(const YV12_BUFFER_CONFIG *yuv,
                    YV12_BUFFER_CONFIG *xyb);
void aom_xyb_to_yuv(const YV12_BUFFER_CONFIG *xyb,
                    YV12_BUFFER_CONFIG *yuv);
```

Good:

```cpp
// Note that:
// 1. When converting between YUV and RGB, these functions use BT.709 matrix
//    coefficients and assume limited-range Y, U, V samples.
// 2. When converting between RGB and XYB, these functions assume the RGB
//    color space is sRGB.
void aom_yuv_to_xyb(const YV12_BUFFER_CONFIG *yuv,
                    YV12_BUFFER_CONFIG *xyb);
void aom_xyb_to_yuv(const YV12_BUFFER_CONFIG *xyb,
                    YV12_BUFFER_CONFIG *yuv);
```
