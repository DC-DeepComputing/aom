---
name: libaom/assign-output-arrays-directly-avoid-memcpy
description: When populating caller-provided output arrays or buffers, assign elements directly into the destination pointer instead of constructing a temporary stack array and calling memcpy
rubric: safety
severity: nit
---

**Rule.** Populate caller-provided output arrays or buffer parameters directly
via index assignment into the destination buffer rather than initializing an
intermediate local stack array and copying it with memcpy(). Assign zero
directly to trailing or unused destination array elements.

**Why.** Constructing an intermediate array on the stack and copying it with
memcpy() causes unnecessary stack frame allocation and redundant memory copy
overhead. Direct assignment populates the caller's buffer in-place with fewer
instructions and avoids unnecessary copies.

## Examples

Bad:

```cpp
void get_transform_matrix(int32_t *output_wmmat, int a, int b, int c, int d,
                          int e, int f) {
  int32_t wmmat[8] = { a, b, c, d, e, f, 0, 0 };
  memcpy(output_wmmat, wmmat, sizeof(int32_t) * 8);
}
```

Good:

```cpp
void get_transform_matrix(int32_t *output_wmmat, int a, int b, int c, int d,
                          int e, int f) {
  output_wmmat[0] = a;
  output_wmmat[1] = b;
  output_wmmat[2] = c;
  output_wmmat[3] = d;
  output_wmmat[4] = e;
  output_wmmat[5] = f;
  output_wmmat[6] = output_wmmat[7] = 0;
}
```
