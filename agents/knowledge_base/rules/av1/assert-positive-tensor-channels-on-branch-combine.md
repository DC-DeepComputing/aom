---
name: libaom/av1/assert-positive-tensor-channels-on-branch-combine
description: When combining or concatenating branch tensors in CNN layers, assert that branch tensors have positive channel counts before concatenating or accumulating
rubric: correctness
severity: should_fix
---

**Rule.** When combining or concatenating branch tensors (or computing
cumulative branch channels) in CNN layers, assert that each branch tensor has a
strictly positive channel count (e.g., assert(tensor->channels > 0) or
assert(channels_per_branch[b] > 0)) prior to concatenation or accumulation. Do
not rely solely on spatial dimension checks (such as check_tensor_equal_dims),
which verify only width and height.

**Why.** Spatial dimension equality checks only verify matching width and
height, leaving zero-channel or uninitialized branch tensors undetected.
Explicitly asserting positive channels catches unallocated or inactive branches
early in debug builds before invalid memory offsets or corrupt tensor
accumulations occur.

## Examples

Bad:

```cpp
if ((cnn_config->layer_config[layer].branches_to_combine & (1 << b)) &&
    b != branch) {
  assert(check_tensor_equal_dims(&tensor2[b], &tensor2[branch]));
  concat_tensor(&tensor2[b], &tensor2[branch]);
}
```

Good:

```cpp
if ((cnn_config->layer_config[layer].branches_to_combine & (1 << b)) &&
    b != branch) {
  assert(check_tensor_equal_dims(&tensor2[b], &tensor2[branch]));
  assert(tensor2[b].channels > 0);
  concat_tensor(&tensor2[b], &tensor2[branch]);
}
```
