---
name: libaom/omit-redundant-switch-cases-matching-default
description: When writing switch statements with a default branch, omit explicit case labels whose handling duplicates the default case behavior
rubric: readability
severity: nit
---

**Rule.** Do not write explicit case labels in a switch statement when their
execution logic is identical to the default branch; let those cases be handled
directly by default. If multiple distinct non-default cases share logic, group
their case labels together instead of duplicating bodies.

**Why.** Duplicating the default branch implementation across explicit case
labels introduces redundant boilerplate, increases maintenance overhead, and
risks desynchronization when the default behavior is modified.

## Examples

Bad:

```cpp
switch (tx_type) {
  case DCT_DCT:
    lowbd_inv_txfm2d_add_no_identity_neon(input, output, stride, tx_type,
                                          tx_size, eob);
    break;
  case IDTX:
    lowbd_inv_txfm2d_add_idtx_neon(input, output, stride, tx_type, tx_size,
                                   eob);
    break;
  default:
    lowbd_inv_txfm2d_add_no_identity_neon(input, output, stride, tx_type,
                                          tx_size, eob);
    break;
}
```

Good:

```cpp
switch (tx_type) {
  case IDTX:
    lowbd_inv_txfm2d_add_idtx_neon(input, output, stride, tx_type, tx_size,
                                   eob);
    break;
  default:
    lowbd_inv_txfm2d_add_no_identity_neon(input, output, stride, tx_type,
                                          tx_size, eob);
    break;
}
```
