---
name: libaom/no-commented-out-debug-code
description: 'Remove commented-out code, prototype declarations, disabled #if 0 or forced conditions (&& 0, || 1), temporary debug prints, and transient assertions before submitting.'
rubric: readability
severity: should_fix
---

**Rule.** Do not leave commented-out code, prototype implementations, obsolete
declarations (such as DECLARE_ALIGNED or unused variables), inactive macros,
disabled blocks (#if 0), forced debug conditionals (such as && 0, && false, ||
1, || true, or artificial if (false) branches), scratch file I/O (fopen,
fprintf), temporary debug prints (printf, puts, putchar, fputc, std::cout,
std::cerr), or transient development assertions (such as
assert(error_info->setjmp == 1) before aom_internal_error()) in production code.
Specifically:
1. Delete dead, replaced, or retired code, declarations, and prototypes completely instead of commenting them out; rely on version control (git history) to preserve previous implementations. If work is intentionally deferred for a future change, track it with an explicit, attributed TODO (// TODO(aomedia:1234): ...) rather than disabled code fragments.
2. Remove scratch debug prints and file logging before submitting. If diagnostic logging or instrumentation is necessary for internal debugging, guard it strictly under dedicated #if CONFIG_*_DEBUG compile flags (e.g., #if CONFIG_RD_DEBUG).
3. Remove temporary boolean overrides (&& 0, || 1, #if 0) injected during development to force fallback paths or disable features.
4. Remove transient development assertions (such as checking error_info->setjmp == 1 before error recovery), ensuring assertions check permanent invariants rather than scaffolding.

**Why.** Commented-out code, dead prototypes, disabled blocks, and ad-hoc debug
statements add visual clutter, degrade code readability, and create ambiguity
over whether logic is active, obsolete, or temporarily disabled. Over time,
disabled fragments rot as surrounding APIs, types, and data structures evolve.
Version control cleanly preserves the complete revision history, making
commented-out fallback code unnecessary. Temporary debug prints pollute
stdout/stderr and degrade performance. Forced conditions (such as && 0 or || 1)
create silent dead branches and risk breaking production behavior if committed.
Transient assertions (such as assert(error_info->setjmp == 1)) are redundant
because error-handling frameworks like aom_internal_error() already validate
info->setjmp before calling longjmp(). Eliminating development scaffolding
prevents bitrot and review noise.

## Examples

Bad:

```cpp
// Variant 1: Commented-out prototype implementation or declarations
/*static INLINE unsigned int horizontal_add_16x8_new(const uint16x8_t vec_16x8) {
  const uint32x4_t a = vpaddlq_u16(vec_16x8);
  return (uint32_t)vadd_u64(vget_low_u64(b), vget_high_u64(b));
}*/
// DECLARE_ALIGNED(16, int32_t, buf[4 * RESTORATION_PROC_UNIT_PELS]);

// Variant 2: Scratch debug prints, file logging, or #if 0 blocks
printf("frame_offset: %d, tx_size: %d\n", cm->frame_offset, tx_size);
#if 0
static void y4m_convert_42xmpeg2(y4m_input *_y4m, unsigned char *_dst) { ... }
#endif

// Variant 3: Temporary forced conditions or disabled branches
if ((mask_16x16 & two_block_mask) == two_block_mask && 0) {
  aom_lpf_horizontal_14_dual(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                             lfin->mblim, lfin->lim, lfin->hev_thr);
}

// Variant 4: Transient debugging assertions before error recovery
assert(td->mb.e_mbd.error_info->setjmp == 1);
if (aom_stop_encode(&mode_bc) < 0) {
  aom_internal_error(td->mb.e_mbd.error_info, AOM_CODEC_ERROR, "Write error");
}
```

Good:

```cpp
// Variant 1: Keep active implementation; git history preserves prototypes
static INLINE unsigned int horizontal_add_16x8(const uint16x8_t vec_16x8) {
  return (uint32_t)vaddlvq_u16(vec_16x8);
}
int32_t *buf = (int32_t *)aom_memalign(
    16, 4 * sizeof(*buf) * RESTORATION_PROC_UNIT_PELS);

// Variant 2: Diagnostic prints removed or guarded under debug flags
#if CONFIG_RD_DEBUG
printf("frame_offset: %d, tx_size: %d\n", cm->frame_offset, tx_size);
#endif  // CONFIG_RD_DEBUG

// Variant 3: Condition evaluates only intentional runtime predicates
if ((mask_16x16 & two_block_mask) == two_block_mask) {
  aom_lpf_horizontal_14_dual(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                             lfin->mblim, lfin->lim, lfin->hev_thr);
}

// Variant 4: Call error handler directly without redundant setjmp assertion
if (aom_stop_encode(&mode_bc) < 0) {
  aom_internal_error(td->mb.e_mbd.error_info, AOM_CODEC_ERROR, "Write error");
}
```
