---
name: libaom/round-floating-point-before-integer-cast
description: When converting continuous floating-point formulas, coordinates, or scaling factors to integer values or pixel grid indices, use round(), roundf(), or rint() instead of truncating (int) casts
rubric: correctness
severity: should_fix
---

**Rule.** When converting continuous floating-point calculation results (such as
mathematical models, rate control formulas, scaling factors, quantizer offsets,
or coordinates) or mapping continuous coordinates to discrete pixel grid indices
for nearest-neighbor reference sampling and lookups, use round(), roundf(), or
rint() before casting to an integer type. Do not cast floating-point expressions
directly to (int) unless truncation toward zero or floor semantics are
explicitly required by specification or algorithm design. Additionally, when
computing linear buffer offsets from rounded grid coordinates, ensure boundary
conditions are explicitly handled by clamping or wrapping boundary indices to
valid frame dimensions.

**Why.** Directly casting floating-point expressions to (int) truncates
fractional values toward zero rather than rounding to the nearest integer. In
continuous mathematical models, rate control heuristics, and DSP formulas,
truncation introduces systematic directional bias, distorts boundary values, and
accumulates precision errors. In nearest-neighbor reference block sampling,
truncation introduces an asymmetric half-pixel bias that degrades prediction
accuracy; explicit rounding identifies the true nearest pixel center, while
boundary checks prevent out-of-bounds buffer access.

## Examples

Bad:

```cpp
// Bad: Direct integer cast truncates toward zero and loses precision
int boost = -(int)((base_qindex + 544.0) * (target_qindex - base_qindex) /
                   (255.0 + 1024.0));
const int estimated_bits = (int)(mb_count * bits_per_mb_fp);

// Bad: Truncates coordinates toward zero, biasing nearest-neighbor sampling
pos_ref = (int)x_ref + (int)y_ref * ref_frame_stride;
pred_block[pos_pred] = ref_frame[pos_ref];
```

Good:

```cpp
// Good: Explicit rounding to nearest integer before casting to int
int boost = -(int)round((base_qindex + 544.0) *
                        (target_qindex - base_qindex) / (255.0 + 1024.0));
const int estimated_bits = (int)roundf(mb_count * bits_per_mb_fp);

// Good: Rounds to nearest pixel center and handles frame boundaries
int x = (int)round(x_ref);
int y = (int)round(y_ref);
if (x == frame_width) x = 0;
if (y == frame_height) y = frame_height - 1;
pos_ref = x + y * ref_frame_stride;
pred_block[pos_pred] = ref_frame[pos_ref];
```
