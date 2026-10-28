---
name: libaom/include-parameter-names-in-function-declarations
description: When declaring function prototypes, forward declarations, or function pointer typedefs, include explicit, descriptive parameter names for all arguments rather than omitting them
rubric: readability
severity: nit
---

**Rule.** In function declarations, forward prototypes, and function pointer
typedefs, provide explicit and descriptive parameter names for all arguments
rather than specifying only their types. Do not omit parameter names or mix
named and unnamed parameters within the same signature, especially across
multiple pointer, buffer, or scalar arguments of identical types (such as input
vs. output buffers or width vs. height dimensions).

**Why.** Omitting parameter names in declarations and typedefs obscures the role
of each argument (such as distinguishing input buffers from output buffers or
width from height) and forces callers and implementers to consult function
definitions or guess parameter contracts.

## Examples

Bad:

```cpp
// Unnamed or partially named parameters in prototypes and typedefs
void aom_fft1d_32_float(const float *, float *, int stride);
typedef void (*aom_fft_1d_func_t)(const float *, float *, int);
```

Good:

```cpp
// Explicit, descriptive parameter names for all arguments
void aom_fft1d_32_float(const float *input, float *output, int stride);
typedef void (*aom_fft_1d_func_t)(const float *input, float *output, int stride);
```
