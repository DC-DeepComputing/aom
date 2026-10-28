---
name: libaom/assert-positive-divisors-before-division
description: When performing division or modulo operations with variable divisors such as step sizes, strides, or dimensions, assert that divisors are strictly positive
rubric: correctness
severity: should_fix
---

**Rule.** Assert that variable divisors (such as step sizes, strides, skip
factors, block sizes, or dimensions) are strictly positive (or non-zero) prior
to executing division or modulo operations. When configuration parameters or
calculated values are used in division arithmetic, add precondition assertions
(e.g., `assert(step > 0)`) to catch zero or uninitialized values early in debug
builds.

**Why.** In C, division or modulo by zero causes undefined behavior and runtime
crashes (SIGFPE). Precondition assertions on divisors catch zero, negative, or
uninitialized values during debug testing before invalid calculations propagate.

## Examples

Bad:

```cpp
static void find_layer_output_size(int in_width, int in_height,
                                   const CNN_LAYER_CONFIG *layer_config,
                                   int *out_width, int *out_height) {
  // Divisors used directly without asserting positive values
  *out_width =
      (in_width + layer_config->skip_width - 1) / layer_config->skip_width;
  *out_height =
      (in_height + layer_config->skip_height - 1) / layer_config->skip_height;
}
```

Good:

```cpp
static void find_layer_output_size(int in_width, int in_height,
                                   const CNN_LAYER_CONFIG *layer_config,
                                   int *out_width, int *out_height) {
  assert(layer_config->skip_width > 0);
  assert(layer_config->skip_height > 0);
  *out_width =
      (in_width + layer_config->skip_width - 1) / layer_config->skip_width;
  *out_height =
      (in_height + layer_config->skip_height - 1) / layer_config->skip_height;
}
```
