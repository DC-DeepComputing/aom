---
name: libaom/avoid-redundant-parameters-available-in-struct-arguments
description: When declaring or modifying functions accepting context, state, or buffer structs, avoid passing separate parameters for properties already accessible through those struct arguments.
rubric: design
severity: should_fix
---

**Rule.** Do not pass separate function parameters for fields, properties, or
flags (such as bit_depth, subsampling_x, subsampling_y, frame_width, or
frame_height) that can be accessed directly through an existing context, state,
or buffer struct argument (such as AV1Common *cm, AV1_COMP *cpi,
YV12_BUFFER_CONFIG *, or aom_image_t *). Access the required values directly
from the struct parameter within the function body instead.

**Why.** Passing redundant standalone parameters bloats function signatures,
clutters call sites, creates multiple sources of truth, forces callers to unpack
struct fields, and risks inconsistency if passed arguments diverge from the
struct's internal state.

## Examples

Bad:

```cpp
// Bad: Passing fields separately when already accessible in context or buffer structs
void av1_set_quantizer(struct AV1Common *const cm, int min_qmlevel,
                       int max_qmlevel, int q, int enable_chroma_deltaq,
                       int enable_hdr_deltaq, bool is_allintra,
                       int subsampling_x, int subsampling_y);

int aom_calc_butteraugli(const YV12_BUFFER_CONFIG *source,
                         const YV12_BUFFER_CONFIG *distorted, int bit_depth,
                         aom_matrix_coefficients_t matrix_coefficients,
                         aom_color_range_t color_range, float *dist_map);
```

Good:

```cpp
// Good: Access subsampling_x/subsampling_y and bit_depth directly via struct arguments
void av1_set_quantizer(struct AV1Common *const cm, int min_qmlevel,
                       int max_qmlevel, int q, int enable_chroma_deltaq,
                       int enable_hdr_deltaq, bool is_allintra);

int aom_calc_butteraugli(const YV12_BUFFER_CONFIG *source,
                         const YV12_BUFFER_CONFIG *distorted,
                         aom_matrix_coefficients_t matrix_coefficients,
                         aom_color_range_t color_range, float *dist_map);
```
