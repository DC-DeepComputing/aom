---
name: libaom/av1/consolidate-identity-translation-into-warping-helpers
description: When implementing motion warping transformation helpers, consolidate identity and translational cases into the general warping function rather than creating separate specialized entry points
rubric: design
severity: should_fix
---

**Rule.** When implementing or updating motion warping transformations,
consolidate identity and translational motion models into the general motion
warping transformation helper rather than defining separate specialized
functions for them. Structure the general helper to handle identity and
translation as degenerate cases of general warping.

**Why.** Identity and translation models are mathematical subsets of affine and
rotzoom warping models. Handling them within a unified warping transformation
function avoids proliferating specialized entry points, reduces API redundancy,
and simplifies caller logic across encoder and decoder pipelines.

## Examples

Bad:

```cpp
// Separate specialized functions for translational and warping transformations
int av1_warp_rotation(MB_MODE_INFO *mi, int8_t rotation, int mi_x, int mi_y);
void av1_translational_rotation(MB_MODE_INFO *mi, int8_t rotation, int mi_x,
                                int mi_y, int32_t *output_wmmat);
```

Good:

```cpp
// Consolidated function handling identity, translation, and general warping
void av1_warp_rotation(MB_MODE_INFO *mi, int8_t rotation, int mi_x, int mi_y);
```
