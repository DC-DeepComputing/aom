---
name: libaom/use-local-parameter-copy-consistently
description: When copying a function parameter to a local working variable or volatile copy, use the local copy consistently rather than referencing the original parameter
rubric: readability
severity: should_fix
---

**Rule.** When a function copies an input parameter into a local working
variable (such as a volatile local copy before setjmp() or a mutable local
copy), use the local variable consistently throughout the function. Do not
reference the original parameter after initializing its local copy.

**Why.** Referencing the original parameter instead of the local copy can cause
inconsistencies, bypass local mutations, or violate compiler guarantees required
for volatile variables across setjmp/longjmp calls.

## Examples

Bad:

```cpp
volatile aom_enc_frame_flags_t flags = enc_flags;
if (setjmp(ppi->error.jmp)) {
  return ppi->error.error_code;
}
ppi->b_calculate_psnr = (enc_flags & AOM_EFLAG_CALCULATE_PSNR) ||
                        (ctx->base.init_flags & AOM_CODEC_USE_PSNR);
```

Good:

```cpp
volatile aom_enc_frame_flags_t flags = enc_flags;
if (setjmp(ppi->error.jmp)) {
  return ppi->error.error_code;
}
ppi->b_calculate_psnr = (flags & AOM_EFLAG_CALCULATE_PSNR) ||
                        (ctx->base.init_flags & AOM_CODEC_USE_PSNR);
```
