---
name: libaom/cache-tensor-buffer-pointers-before-loops
description: When accessing model tensor data in pixel or sample loops, cache buffer pointers in local variables before loops instead of repeatedly querying interpreter accessors
rubric: safety
severity: should_fix
---

**Rule.** When reading or writing model tensor data across pixel, block, or
sample loops, cache tensor buffer pointers (such as from
typed_output_tensor<T>(), typed_input_tensor<T>(), or TfLiteTensorData()) in a
local pointer variable before entering the loops. Do not repeatedly invoke
tensor accessor methods or query tensor data buffers inside inner loop
iterations.

**Why.** Invoking tensor accessor methods inside hot pixel loops introduces
unnecessary function call, tensor lookup, and runtime dimension validation
overhead on every iteration, preventing compiler loop optimizations and
degrading video processing throughput.

## Examples

Bad:

```cpp
for (int j = 0; j < bh; ++j) {
  for (int i = 0; i < bw; ++i) {
    comp_pred[i + j * comp_stride] =
        interpreter->typed_output_tensor<float>(0)[i + j * bw];
  }
}
```

Good:

```cpp
const float *output = interpreter->typed_output_tensor<float>(0);
for (int j = 0; j < bh; ++j) {
  for (int i = 0; i < bw; ++i) {
    comp_pred[i + j * comp_stride] = output[i + j * bw];
  }
}
```
