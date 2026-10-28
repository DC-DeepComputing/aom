---
name: libaom/document-non-obvious-logic-and-name-magic-values
description: When implementing non-obvious algorithms, mathematical workarounds, or magic initialization offsets, document the logic with comments and assign values to named constants or variables
rubric: readability
severity: nit
---

**Rule.** Document non-obvious algorithms, mathematical workarounds, or magic
initialization/offset values with explanatory comments, and assign numeric
literals to named constants or local const variables rather than repeating
hardcoded numbers across multiple operations. Clarify why the workaround is
necessary (such as biasing unsigned buffers to recover signed residuals) and how
the named constant is applied.

**Why.** Hardcoded numeric literals and unexplained algorithmic workarounds
obscure code intent and make maintenance error-prone. Documenting the rationale
and giving values meaningful names clarifies operational invariants and ensures
values can be updated consistently.

## Examples

Bad:

```cpp
// Hardcoded offset used across buffer initialization and residual recovery
for (int r = 0; r < txh; ++r) {
  for (int c = 0; c < txw; ++c) {
    recon_diffu[r * MAX_TX_SIZE + c] = 1023;
  }
}
av1_highbd_inv_txfm_add(input, recon_diffu, stride, ...);
for (int r = 0; r < txh; ++r) {
  for (int c = 0; c < txw; ++c) {
    dst[r * stride + c] = (int16_t)recon_diffu[r * MAX_TX_SIZE + c] - 1023;
  }
}
```

Good:

```cpp
// Initialize residual buffer with a 1023 bias so unsigned highbd_inv_txfm_add()
// can process signed differences without underflow, then subtract the bias.
const uint16_t residual_init_val = 1023;
for (int r = 0; r < txh; ++r) {
  for (int c = 0; c < txw; ++c) {
    residual16[r * MAX_TX_SIZE + c] = residual_init_val;
  }
}
av1_highbd_inv_txfm_add(input, residual16, stride, ...);
for (int r = 0; r < txh; ++r) {
  for (int c = 0; c < txw; ++c) {
    dst[r * stride + c] =
        (int16_t)residual16[r * MAX_TX_SIZE + c] - residual_init_val;
  }
}
```
