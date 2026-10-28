---
name: libaom/do-not-reset-caller-error-info-in-component-helpers
description: When writing or modifying component reset or initialization helpers, do not reset caller-owned struct aom_internal_error_info or clear its error_code field.
rubric: design
severity: should_fix
---

**Rule.** Do not reset or clear caller-owned struct aom_internal_error_info
(such as setting error_info->error_code = AOM_CODEC_OK) inside lower-level
component reset or initialization functions. Managing and clearing the error
context lifecycle belongs exclusively to the caller that establishes the setjmp
frame.

**Why.** struct aom_internal_error_info encapsulates non-local jump recovery via
jmp_buf and associated error message details (has_detail, detail). Resetting
error_code in subroutines violates caller error lifecycle management and leaves
stale error detail strings inconsistent.

## Examples

Bad:

```cpp
void od_ec_enc_reset(od_ec_enc *enc) {
  enc->offs = 0;
  enc->low = 0;
  enc->rng = 0x8000;
  enc->cnt = -9;
  enc->error_info->error_code = AOM_CODEC_OK;
}
```

Good:

```cpp
void od_ec_enc_reset(od_ec_enc *enc) {
  enc->offs = 0;
  enc->low = 0;
  enc->rng = 0x8000;
  enc->cnt = -9;
}
```
