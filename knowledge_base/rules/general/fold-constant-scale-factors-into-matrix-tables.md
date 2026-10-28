---
name: libaom/fold-constant-scale-factors-into-matrix-tables
description: When evaluating transforms with constant matrices or lookup tables, fold fixed scaling factors directly into table coefficients instead of multiplying or dividing at runtime
rubric: safety
severity: should_fix
---

**Rule.** When applying constant transform matrices, filter kernels, or offset
tables, fold fixed scale factors directly into the precomputed table entries
rather than applying redundant multiplications or divisions at each evaluation
step. Ensure table constants and offsets match the operational range of the
input and output variables directly.

**Why.** Multiplying or dividing by a constant scale factor inside inner pixel
processing loops introduces redundant arithmetic instructions and floating-point
operations. Pre-scaling table coefficients simplifies expressions, eliminates
runtime operations, and improves throughput in evaluation loops.

## Examples

Bad:

```cpp
// Redundant division in constant table and cancelled scaling in inner loop
const float RGBtoYUVMatrix[9] = {
  .183f / 255.f, .614f / 255.f, .062f / 255.f,
  -.101f / 255.f, -.338f / 255.f, .439f / 255.f,
  .439f / 255.f, -.399f / 255.f, -.040f / 255.f
};
const float RGBtoYUVMatrixAdd[3] = { 16.f / 255.f, 128.f / 255.f, 128.f / 255.f };

*y = ClampRound8(
    255.0f * (RGBtoYUVMatrixAdd[0] + RGBtoYUVMatrix[0] * (float)r +
              RGBtoYUVMatrix[1] * (float)g + RGBtoYUVMatrix[2] * (float)b));
```

Good:

```cpp
// Scale factor folded directly into static constant matrix and offset tables
static const float RGBtoYUVMatrix[9] = {
  .183f, .614f, .062f,
  -.101f, -.338f, .439f,
  .439f, -.399f, -.040f
};
static const float RGBtoYUVMatrixAdd[3] = { 16.f, 128.f, 128.f };

*y = ClampRound8(RGBtoYUVMatrixAdd[0] + RGBtoYUVMatrix[0] * (float)r +
                 RGBtoYUVMatrix[1] * (float)g + RGBtoYUVMatrix[2] * (float)b);
```
