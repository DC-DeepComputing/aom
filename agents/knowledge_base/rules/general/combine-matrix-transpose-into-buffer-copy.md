---
name: libaom/combine-matrix-transpose-into-buffer-copy
description: When copying and transposing 2D matrix buffers, index transposed coordinates directly during the copy loop instead of copying sequentially and swapping in place
rubric: safety
severity: should_fix
---

**Rule.** When transferring or copying a 2D matrix or block from an intermediate
buffer to an output buffer and transposing it, combine the transpose index
permutation directly into the copy pass (e.g., dst[i * N + j] = src[j * N + i]).
Do not perform a sequential 1D buffer copy followed by a separate in-place
transposition swap loop.

**Why.** Performing an in-place matrix transposition after copying requires an
extra full-buffer traversal and redundant element swap operations. Fusing
transposition directly into the destination assignment minimizes memory
operations, improves data locality, and simplifies the code.

## Examples

Bad:

```cpp
// Bad: Sequential copy followed by an extra in-place transpose pass
for (int idx = 0; idx < 16; ++idx) coeff[idx] = (tran_low_t)buffer2[idx];

for (int i = 0; i < 4; i++) {
  for (int j = i + 1; j < 4; j++) {
    tran_low_t temp = coeff[j * 4 + i];
    coeff[j * 4 + i] = coeff[i * 4 + j];
    coeff[i * 4 + j] = temp;
  }
}
```

Good:

```cpp
// Good: Combine transposition directly into the buffer copy pass
for (int i = 0; i < 4; i++) {
  for (int j = 0; j < 4; j++) {
    coeff[i * 4 + j] = (tran_low_t)buffer2[j * 4 + i];
  }
}
```
