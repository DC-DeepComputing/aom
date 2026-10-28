---
name: libaom/avoid-redundant-input-buffer-copies
description: When passing input buffers to downstream functions that do not mutate data, pass existing buffers directly instead of allocating temporary stack arrays and copying with memcpy
rubric: safety
severity: should_fix
---

**Rule.** Do not allocate temporary stack or scratch buffers (e.g.
`DECLARE_ALIGNED`, local arrays) and copy data via `memcpy`, `memmove`, or
element-by-element loops when the existing input buffer already matches the
expected data layout and type and the callee does not mutate it. Pass the
existing input buffer pointer directly to downstream processing functions.

**Why.** Allocating unnecessary stack or scratch arrays and copying input
buffers wastes CPU cycles, increases memory bandwidth and cache footprint, and
unnecessarily inflates stack frame size without providing any benefit.

## Examples

Bad:

```cpp
case TX_16X64: {
  int32_t mod_input[16 * 64];
  memcpy(mod_input, input, 16 * 32 * sizeof(*mod_input));
  lowbd_inv_txfm2d_add_universe_neon(mod_input, output, stride, tx_type,
                                     tx_size, eob);
} break;
case TX_32X64: {
  DECLARE_ALIGNED(16, int32_t, mod_input[32 * 64]);
  memcpy(mod_input, input, 32 * 32 * sizeof(*mod_input));
  lowbd_inv_txfm2d_add_universe_neon(mod_input, output, stride, tx_type,
                                     tx_size, eob);
} break;
```

Good:

```cpp
case TX_16X64: {
  lowbd_inv_txfm2d_add_universe_neon(input, output, stride, tx_type,
                                     tx_size, eob);
} break;
case TX_32X64: {
  lowbd_inv_txfm2d_add_universe_neon(input, output, stride, tx_type,
                                     tx_size, eob);
} break;
```
