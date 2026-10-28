---
name: libaom/aom_dsp/guard-extended-partition-dsp-with-realtime-only
description: 'When defining, instantiating, or testing DSP kernels for 1:4 and 4:1 extended partition block sizes, guard them with #if !CONFIG_REALTIME_ONLY'
rubric: correctness
severity: should_fix
---

**Rule.** Guard DSP function definitions, macro instantiations, and test suites
for 1:4 and 4:1 extended partition block sizes (4x16, 16x4, 8x32, 32x8, 16x64,
64x16) with '#if !CONFIG_REALTIME_ONLY'. Do not define or instantiate extended
partition kernels unconditionally when real-time mode excludes them.

**Why.** In libaom, extended partition block sizes (1:4 and 4:1 aspect ratios)
are disabled in realtime-only encoding mode and excluded from
'@encoder_block_sizes' in aom_dsp_rtcd_defs.pl under CONFIG_REALTIME_ONLY.
Guarding their kernel implementations and test instantiations avoids compiling
unused code and prevents mismatch with RTCD declarations in realtime builds.

## Examples

Bad:

```cpp
MASKED_SAD_WXH_NEON(128, 128)
MASKED_SAD_WXH_NEON(4, 16)
MASKED_SAD_WXH_NEON(16, 4)
MASKED_SAD_WXH_NEON(8, 32)
MASKED_SAD_WXH_NEON(32, 8)
MASKED_SAD_WXH_NEON(16, 64)
MASKED_SAD_WXH_NEON(64, 16)
```

Good:

```cpp
MASKED_SAD_WXH_NEON(128, 128)
#if !CONFIG_REALTIME_ONLY
MASKED_SAD_WXH_NEON(4, 16)
MASKED_SAD_WXH_NEON(16, 4)
MASKED_SAD_WXH_NEON(8, 32)
MASKED_SAD_WXH_NEON(32, 8)
MASKED_SAD_WXH_NEON(16, 64)
MASKED_SAD_WXH_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY
```
