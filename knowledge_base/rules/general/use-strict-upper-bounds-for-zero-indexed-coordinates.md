---
name: libaom/use-strict-upper-bounds-for-zero-indexed-coordinates
description: When asserting or validating 0-indexed coordinate and pixel bounds, use strict upper bounds (coord < dimension) and non-negative lower bounds instead of <= dimension
rubric: correctness
severity: should_fix
---

**Rule.** When checking or asserting 0-indexed coordinate, pixel, or buffer
index bounds, validate that coordinates are strictly less than the dimension
(coord < dimension) and non-negative (coord >= 0). Do not use non-strict upper
bounds (coord <= dimension), which allow off-by-one out-of-bounds accesses.

**Why.** In 0-indexed coordinate and indexing systems spanning [0, dimension -
1], a coordinate equal to dimension is out of bounds. Using <= allows an
off-by-one index equal to dimension, leading to buffer overruns, memory
corruption, or invalid projection and interpolation math.

## Examples

Bad:

```cpp
// Incorrect: allows off-by-one when x == width or y == height, and misses lower bounds
assert(x <= width && y <= height);
```

Good:

```cpp
// Correct: verifies valid [0, dimension - 1] range for 0-indexed coordinates
assert(x >= 0 && x < width && y >= 0 && y < height);
```
