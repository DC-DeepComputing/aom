---
name: libaom/prefer-while-over-do-while-for-buffer-io
description: When implementing buffer or I/O reading helpers, use while or for loops checking remaining size upfront instead of do-while to handle zero-length inputs safely
rubric: correctness
severity: nit
---

**Rule.** When implementing buffer, stream, or file I/O reading loops, use while
or for loops that check remaining length or bounds prior to loop execution (such
as while (len < size && ...)) rather than do-while loops. Ensure that
zero-length requests (size == 0) terminate immediately without performing I/O
operations or evaluating pointer arithmetic on potentially null buffers.

**Why.** A do-while loop executes its body at least once even when size == 0.
When size is 0 and buf is NULL, pointer offset calculations such as (uint8_t
*)buf + len evaluate NULL + 0, which is undefined behavior in C/C++ and triggers
UBSan diagnostics.

## Examples

Bad:

```cpp
size_t len = 0;
do {
  const size_t n = fread((uint8_t *)buf + len, 1, size - len, file);
  len += n;
  // If size == 0 and buf == NULL, (NULL + 0) evaluates in loop body
} while (!feof(file) && len < size && ++try_count < kMaxTries);
```

Good:

```cpp
size_t len = 0;
while (!feof(file) && len < size && try_count < kMaxTries) {
  const size_t n = fread((uint8_t *)buf + len, 1, size - len, file);
  ++try_count;
  len += n;
}
```
