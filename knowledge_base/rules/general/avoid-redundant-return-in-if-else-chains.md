---
name: libaom/avoid-redundant-return-in-if-else-chains
description: In void functions or terminal if-else chains, do not add redundant return statements or empty no-op branches for cases requiring no action
rubric: readability
severity: should_fix
---

**Rule.** Do not add redundant 'return;' statements or dedicated no-op branches
inside 'if ... else if' chains in void functions when subsequent branches are
mutually exclusive and no executable code follows. Omit the no-op case or use
assertions for unsupported branches instead of creating dead control flow.

**Why.** In an 'if ... else if' chain with no trailing code, mutually exclusive
branches already skip subsequent conditions and return naturally at the end of
the function. Adding a branch solely to perform an early return adds redundant
branching and dead control flow.

## Examples

Bad:

```cpp
void av1_cnn_activate_c(float **output, int channels, int width, int height,
                        int stride, ACTIVATION layer_activation) {
  if (layer_activation == NONE) {
    // For layer_activation of type NONE, no operation needs to be done.
    return;
  } else if (layer_activation == RELU) {
    // ...
  }
}
```

Good:

```cpp
void av1_cnn_activate_c(float **output, int channels, int width, int height,
                        int stride, ACTIVATION layer_activation) {
  if (layer_activation == RELU) {
    // ...
  } else if (layer_activation == SIGMOID) {
    assert(0 && "Sigmoid has not been supported in CNN.");
  } else if (layer_activation != NONE) {
    assert(0 && "Unknown activation type");
  }
}
```
