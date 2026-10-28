---
name: libaom/use-descriptive-names-for-counters-not-flags
description: When declaring variables that count items, packets, or iterations, use descriptive counter names like packet_count or num_items rather than flags or mask
rubric: readability
severity: nit
---

**Rule.** Do not name counter, accumulator, or size variables with
bitmask-suggestive terms such as `flags` or `mask`. Use descriptive counter
identifiers that explicitly reflect the entity being counted (such as
`packet_count`, `num_packets`, `frame_count`, or `num_frames`).

**Why.** In encoder and decoder codebases, `flags` and `mask` conventionally
refer to bitmasks (such as `aom_enc_frame_flags_t` or configuration bitfields).
Using bitmask terminology for scalar count variables creates misleading code
that obscures whether a variable holds discrete bit flags or an integer
quantity.

## Examples

Bad:

```cpp
size_t flags = 0;
while (const aom_codec_cx_pkt_t *pkt = iter.Next()) {
  flags++;
  if (pkt->kind == AOM_CODEC_PSNR_PKT) had_psnr = true;
}
```

Good:

```cpp
size_t packet_count = 0;
while (const aom_codec_cx_pkt_t *pkt = iter.Next()) {
  packet_count++;
  if (pkt->kind == AOM_CODEC_PSNR_PKT) had_psnr = true;
}
```
