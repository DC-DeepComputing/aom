---
name: libaom/av1/validate-tflite-interpreter-builder-status-and-tensor-counts
description: When initializing a TensorFlow Lite Interpreter via InterpreterBuilder, verify that builder(&interpreter) returns kTfLiteOk and validate input/output tensor counts
rubric: correctness
severity: should_fix
---

**Rule.** When constructing a TensorFlow Lite Interpreter using
tflite::InterpreterBuilder (or C API equivalent), verify that building the
interpreter succeeds with kTfLiteOk before proceeding. Additionally, validate
that the interpreter's input and output tensor counts
(interpreter->inputs().size() and interpreter->outputs().size()) match the
expected model schema before allocating tensors or invoking inference.

**Why.** If InterpreterBuilder fails or encounters a corrupted model, continuing
execution risks dereferencing an uninitialized or null interpreter. Validating
tensor counts ensures the loaded model matches the expected input/output
contract before allocating memory or executing inference.

## Examples

Bad:

```cpp
tflite::InterpreterBuilder builder(model, resolver);
std::unique_ptr<tflite::Interpreter> interpreter;
builder(&interpreter);
```

Good:

```cpp
tflite::InterpreterBuilder builder(model, resolver);
std::unique_ptr<tflite::Interpreter> interpreter;
if (builder(&interpreter) != kTfLiteOk || interpreter == nullptr) {
  reporter->Report("Failed to build TFLite interpreter.");
  return nullptr;
}
if (interpreter->inputs().size() != 4 || interpreter->outputs().size() != 1) {
  reporter->Report("Unexpected model tensor counts.");
  return nullptr;
}
```
