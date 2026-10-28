---
name: libaom/avoid-static-scratch-buffers
description: When allocating intermediate scratch buffers in codec routines, do not declare static mutable arrays; use stack, dynamic heap, or caller-provided context buffers to avoid multi-threading data races
rubric: safety
severity: blocking
---

**Rule.** Do not declare static mutable arrays, buffers, or pointers for
intermediate scratch storage in codec processing or DSP routines. Because
routines can execute concurrently across multiple worker threads (such as
tile-based or row-based multithreading), allocate temporary memory dynamically
with aom_memalign() / aom_malloc(), use fixed stack buffers when small, or pass
context-managed scratch buffers from the caller.

**Why.** Static mutable arrays are shared globally across all threads. When
multi-threaded encoding or decoding runs filters, transforms, or restoration
across worker threads concurrently, concurrent writes to shared static buffers
cause data races, memory corruption, and non-deterministic decoding artifacts.

## Examples

Bad:

```cpp
// Static mutable buffers cause data races when called concurrently across threads
static int32_t A_[RESTORATION_PROC_UNIT_PELS] = { 0 };
static int32_t B_[RESTORATION_PROC_UNIT_PELS] = { 0 };
int32_t *square_sum_buf = A_;
int32_t *sum_buf = B_;
```

Good:

```cpp
// Allocate thread-safe dynamic memory or use caller-provided scratch buffers
const size_t buf_size = 2 * sizeof(int32_t) * RESTORATION_PROC_UNIT_PELS;
int32_t *buf = (int32_t *)aom_memalign(16, buf_size);
if (!buf) return -1;
int32_t *square_sum_buf = buf;
int32_t *sum_buf = square_sum_buf + RESTORATION_PROC_UNIT_PELS;
// ... process buffers ...
aom_free(buf);
return 0;
```
