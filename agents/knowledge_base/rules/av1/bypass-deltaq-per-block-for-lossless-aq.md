---
name: libaom/av1/bypass-deltaq-per-block-for-lossless-aq
description: When applying quantizer delta features like chroma or HDR delta-q, bypass adjustments per-block when q == 0 instead of disabling flags globally or asserting q != 0
rubric: correctness
severity: should_fix
---

**Rule.** When configuring quantizer delta features (such as
enable_chroma_deltaq or enable_hdr_deltaq), do not disable flags globally at the
frame level or assert q != 0 during quantizer initialization. Conditionally
bypass delta-q adjustments per block or segment (e.g., in av1_set_quantizer) by
checking q != 0.

**Why.** Under adaptive quantization (AQ) or segmentation, individual blocks can
be lossless (q == 0) while the rest of the frame remains lossy (q > 0).
Disabling delta-q flags at the frame level inappropriately strips delta-q
optimizations from lossy blocks, and asserting q != 0 in quantizer setup
triggers false assertion failures on mixed lossless/lossy frames.

## Examples

Bad:

```cpp
// Asserting delta-q is disabled when q == 0 or checking only the delta-q flag
assert(!enable_chroma_deltaq || q != 0);
if (enable_chroma_deltaq) {
  setup_chroma_delta_q(cm, q);
}
```

Good:

```cpp
// Check q at the block/segment level so delta-q applies to lossy blocks under AQ
if (enable_chroma_deltaq && q) {
  setup_chroma_delta_q(cm, q);
}
```
