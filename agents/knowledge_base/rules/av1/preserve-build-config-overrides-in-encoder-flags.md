---
name: libaom/av1/preserve-build-config-overrides-in-encoder-flags
description: When updating encoder runtime flags dynamically across encode calls, preserve build-time configuration overrides such as CONFIG_INTERNAL_STATS using preprocessor guards
rubric: correctness
severity: should_fix
---

**Rule.** When updating encoder control flags dynamically at runtime (such as
per-frame PSNR calculation via ppi->b_calculate_psnr), preserve build-time
configuration overrides (such as CONFIG_INTERNAL_STATS) using #if CONFIG_*
preprocessor guards. Do not unconditionally overwrite configuration state that
was initialized or required by build-time configuration options.

**Why.** Build configurations like CONFIG_INTERNAL_STATS initialize flags such
as ppi->b_calculate_psnr at primary compressor creation to enable statistics
collection across all frames. Unconditionally overwriting these flags with
dynamic per-frame flags disables required features on frames that do not
explicitly set the flag, breaking stats collection and test builds.

## Examples

Bad:

```cpp
// Overwrites b_calculate_psnr even when CONFIG_INTERNAL_STATS is enabled
ppi->b_calculate_psnr = (flags & AOM_EFLAG_CALCULATE_PSNR) ||
                        (ctx->base.init_flags & AOM_CODEC_USE_PSNR);
```

Good:

```cpp
#if CONFIG_INTERNAL_STATS
assert(ppi->b_calculate_psnr == 1);
#else
ppi->b_calculate_psnr = (ctx->base.init_flags & AOM_CODEC_USE_PSNR) ||
                        (flags & AOM_EFLAG_CALCULATE_PSNR);
#endif  // CONFIG_INTERNAL_STATS
```
