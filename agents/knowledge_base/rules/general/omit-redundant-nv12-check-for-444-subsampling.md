---
name: libaom/omit-redundant-nv12-check-for-444-subsampling
description: When identifying 4:4:4 chroma subsampling via x_chroma_shift and y_chroma_shift on aom_image_t, omit redundant img_fmt != AOM_IMG_FMT_NV12 checks
rubric: readability
severity: nit
---

**Rule.** When identifying 4:4:4 chroma subsampling on an aom_image_t via
`x_chroma_shift == 0 && y_chroma_shift == 0`, omit redundant format inequality
checks such as `img_fmt != AOM_IMG_FMT_NV12`. In libaom, AOM_IMG_FMT_NV12 is
defined as a 4:2:0 semi-planar format with both x_chroma_shift and
y_chroma_shift set to 1, so checking that both chroma shifts equal 0 already
excludes NV12.

**Why.** In libaom, aom_img_alloc() initializes AOM_IMG_FMT_NV12 with
x_chroma_shift = 1 and y_chroma_shift = 1 matching 4:2:0 subsampling. Testing
that both chroma shifts are 0 already evaluates to false for NV12, making
additional format exclusions redundant and misleading about how NV12 subsampling
is represented.

## Examples

Bad:

```cpp
// Redundant format check when inspecting chroma shifts
const bool is_444 = (img.x_chroma_shift == 0 && img.y_chroma_shift == 0) &&
                    img_fmt != AOM_IMG_FMT_NV12;
```

Good:

```cpp
// Chroma shifts alone determine 4:4:4 subsampling (NV12 sets both shifts to 1)
const bool is_444 = (img.x_chroma_shift == 0 && img.y_chroma_shift == 0);
```
