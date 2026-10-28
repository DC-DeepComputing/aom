---
name: libaom/av1/avoid-redundant-transform-buffer-zeroing-beyond-eob
description: When preparing input or intermediate coefficient buffers for 2D transform kernels bounded by eob, omit redundant memset or zeroing of unread trailing rows
rubric: safety
severity: should_fix
---

**Rule.** Do not perform redundant buffer zeroing (such as memset) or copying on
trailing input rows or regions that subsequent transform stages will not access
due to eob (end-of-block) bounds or reduced transform size limits (such as 32x32
active regions in 64x64 transforms). Rely on the transform kernel's internal
zero-generation and eob-bounded loops instead of pre-zeroing unread memory.

**Why.** Inverse transform stages only load rows or columns up to the active
coefficient boundary (eob/eoby) and generate zeros directly into intermediate
buffers for unread lines. Pre-zeroing unread trailing rows in scratch or input
buffers introduces unnecessary memory writes and cache pollution without
affecting output correctness.

## Examples

Bad:

```cpp
case TX_64X64: {
  int32_t mod_input[64 * 64];
  for (row = 0; row < 32; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  // Bad: Redundant memset for trailing rows that inv_txfm2d never reads
  memset(mod_input + 32 * 64, 0, 32 * 64 * sizeof(*mod_input));
  lowbd_inv_txfm2d_add_universe_neon(mod_input, output, stride, tx_type,
                                     tx_size, eob);
  break;
}
```

Good:

```cpp
case TX_64X64: {
  int32_t mod_input[64 * 64];
  for (row = 0; row < 32; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  // Good: Omit pre-zeroing trailing rows; transform kernel is bounded by eob
  lowbd_inv_txfm2d_add_universe_neon(mod_input, output, stride, tx_type,
                                     tx_size, eob);
  break;
}
```
