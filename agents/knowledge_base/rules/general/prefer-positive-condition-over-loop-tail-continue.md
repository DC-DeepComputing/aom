---
name: libaom/prefer-positive-condition-over-loop-tail-continue
description: When executing a single statement at the end of a loop iteration, prefer a direct positive condition over an inverted guard with continue
rubric: readability
severity: nit
---

**Rule.** Prefer a direct positive conditional (`if (cond) statement;`) over an
inverted guard with `continue` (`if (!cond) continue; statement;`) when
executing a single statement at the tail of a loop iteration.

**Why.** Early `continue` with inverted conditions helps reduce indentation for
large, multi-line blocks, but introduces unnecessary control-flow jumps and
cognitive overhead when only a single statement remains at the end of the loop.

## Examples

Bad:

```cpp
while (const aom_codec_cx_pkt_t *pkt = iter.Next()) {
  packets++;
  if (pkt->kind != AOM_CODEC_PSNR_PKT) continue;
  had_psnr = true;
}
```

Good:

```cpp
while (const aom_codec_cx_pkt_t *pkt = iter.Next()) {
  packets++;
  if (pkt->kind == AOM_CODEC_PSNR_PKT) had_psnr = true;
}
```
