---
name: libaom/general-offset-residual-loop-indexing-after-simd
description: When processing residual elements in scalar fallback loops after vectorized chunks, index buffers using the accumulated offset rather than restarting from zero
rubric: correctness
severity: should_fix
---

**Rule.** When processing residual elements in scalar fallback loops after
vectorized blocks, index buffers using the accumulated offset (such as `src[w +
i]` or `for (int i = w; i < w + residual; i++)`) rather than restarting indexing
from zero (`src[i]`). Alternatively, advance the base pointer by the accumulated
offset (`src + w`) before entering the residual processing loop.

**Why.** Vector processing loops advance an offset counter as chunks of elements
are processed. Indexing from zero in subsequent scalar fallback loops re-reads
and overwrites already processed elements at the beginning of the buffer while
leaving the true residual elements unhandled, causing data corruption and
conformance failures.

## Examples

Bad:

```cpp
// Vector loop processed elements up to 'w'
while (width - w >= 8) {
  // SIMD processing...
  w += 8;
}
if (width - w >= 4) {
  for (int i = 0; i < 4; i++) {
    dst_ptr[i] = src_ptr[i];  // Bug: overwrites dst_ptr[0..3]
  }
  w += 4;
}
```

Good:

```cpp
// Vector loop processed elements up to 'w'
while (width - w >= 8) {
  // SIMD processing...
  w += 8;
}
if (width - w >= 4) {
  for (int i = w; i < w + 4; i++) {
    dst_ptr[i] = src_ptr[i];  // Correctly indexes remaining elements at offset 'w'
  }
  w += 4;
}
```
