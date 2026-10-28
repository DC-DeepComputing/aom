---
name: libaom/match-tensor-accessor-template-type-to-model-data-type
description: When querying model tensor buffers with typed accessors like typed_output_tensor<T>() or typed_input_tensor<T>(), ensure template type argument T matches the model's actual tensor data type
rubric: correctness
severity: should_fix
---

**Rule.** When querying or extracting model input and output tensor buffers
using typed accessor methods (such as typed_output_tensor<T>() or
typed_input_tensor<T>()), ensure the template type argument T strictly matches
the underlying data type expected or produced by the model (e.g., float vs. int
or uint8_t). Verify the expected tensor data type from the model specification
or tensor schema before casting or retrieving the buffer pointer.

**Why.** Passing a mismatched template type to tensor accessors leads to invalid
buffer reinterpretation, corrupted model inputs or predictions, or runtime
nullptr dereferences when interpreter type checks fail.

## Examples

Bad:

```cpp
// Model outputs float predictions, but accessor requests int pointer:
const int *output = interpreter->typed_output_tensor<int>(0);
for (int i = 0; i < count; ++i) {
  predictions[i] = output[i];
}
```

Good:

```cpp
// Template type argument matches model tensor data type (float):
const float *output = interpreter->typed_output_tensor<float>(0);
for (int i = 0; i < count; ++i) {
  predictions[i] = output[i];
}
```
