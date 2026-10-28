---
name: libaom/avoid-redundant-zero-init-of-stack-buffers
description: When declaring intermediate stack arrays in DSP or SIMD routines, do not zero-initialize buffers that will be overwritten before reading
rubric: safety
severity: should_fix
---

**Rule.** Do not zero-initialize local stack buffers or SIMD vector arrays (e.g.
using '= { 0 }' or '= { { 0 } }') in DSP or performance-critical routines when
all elements will be loaded or overwritten before being read. Leave the array
uninitialized at declaration so the compiler does not emit redundant zeroing
instructions on function entry.

**Why.** Zero-initializing stack arrays in hot DSP or SIMD paths causes
compilers to emit unnecessary memset calls or store loops on every invocation.
When the buffer is immediately populated by load intrinsics or transform passes,
this zeroing wastes CPU cycles and reduces throughput.

## Examples

Bad:

```cpp
// Redundant zero-initialization of vector buffer in hot DSP routine
int16x8_t intermediate[64 * 8] = { { 0 } };
load_and_process_neon(src, intermediate);
```

Good:

```cpp
// Leave intermediate buffer uninitialized when fully overwritten
int16x8_t intermediate[64 * 8];
load_and_process_neon(src, intermediate);
```
