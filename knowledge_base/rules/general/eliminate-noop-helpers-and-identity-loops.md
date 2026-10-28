---
name: libaom/eliminate-noop-helpers-and-identity-loops
description: When refactoring function dispatch or callback handlers, delete obsolete identity or no-op helpers and skip redundant loops or early return on no-op modes
rubric: readability
severity: should_fix
---

**Rule.** Do not retain obsolete identity or no-op callback helpers when
refactoring function dispatch or transformation pipelines. Delete unused no-op
helpers and handle identity operation modes by early-returning or skipping
buffer iteration rather than executing redundant element-wise loops.

**Why.** Executing nested loops that apply an identity transformation (such as x
= identity(x)) wastes CPU cycles and memory bandwidth across multidimensional
buffers. Deleting unused no-op helpers and bypassing inactive pipeline stages
simplifies control flow and eliminates dead code.

## Examples

Bad:

```cpp
static INLINE float identity(float x) { return x; }

// ...
} else if (layer_activation == NONE) {
  for (int c = 0; c < channels; ++c) {
    for (int i = 0; i < height; ++i) {
      for (int j = 0; j < width; ++j) {
        output[c][i * stride + j] = identity(output[c][i * stride + j]);
      }
    }
  }
}
```

Good:

```cpp
if (layer_activation == NONE) {
  // NONE mode performs an identity transformation; no operation needed.
  return;
}
```
