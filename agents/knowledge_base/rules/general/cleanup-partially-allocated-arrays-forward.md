---
name: libaom/cleanup-partially-allocated-arrays-forward
description: When freeing dynamically allocated array elements on allocation failure in a loop, iterate forward over indices 0 to failed_index - 1 using the cleanup iterator
rubric: safety
severity: should_fix
---

**Rule.** When cleaning up dynamically allocated array elements or 2D buffers
upon an allocation failure inside a loop, iterate forward over only the
successfully allocated elements (indices 0 through failed_index - 1) using the
cleanup loop's iterator variable (e.g., `for (int i = 0; i < row; ++i)
aom_free(buffers[i]);`). Do not reuse the outer failing index variable when
indexing into the array during cleanup, and do not count down backwards from the
failed index.

**Why.** When an allocation fails at index `i`, `buffers[i]` is NULL or
uninitialized while only elements `0` through `i - 1` have been allocated.
Reusing the outer failing index variable (`buffers[i]`) repeatedly frees NULL
while leaking all previously allocated buffers `buffers[0..i-1]`. Counting down
starting from `i` attempts to free the uninitialized or NULL pointer at `i` and
is prone to off-by-one errors and signed loop index underflow.

## Examples

Bad:

```cpp
for (int row = 0; row < num_rows; ++row) {
  buffers[row] = (int *)aom_malloc(sizeof(*buffers[row]) * count);
  if (!buffers[row]) {
    // Bad: Reuses outer 'row' index (leaking buffers[0..row-1]) or counts down from 'row'
    for (int i = row; i >= 0; --i) aom_free(buffers[row]);
    aom_free(buffers);
    return false;
  }
}
```

Good:

```cpp
for (int row = 0; row < num_rows; ++row) {
  buffers[row] = (int *)aom_malloc(sizeof(*buffers[row]) * count);
  if (!buffers[row]) {
    // Good: Forward loop cleaning only previously allocated indices [0, row)
    for (int i = 0; i < row; ++i) aom_free(buffers[i]);
    aom_free(buffers);
    return false;
  }
}
```
