---
name: libaom/group-related-output-param-assignments
description: When assigning to multiple related output pointer parameters in a function, group their dereference assignments together where outputs are produced.
rubric: readability
severity: nit
---

**Rule.** Group assignments to related output pointer parameters together at the
logical point where their values are produced, rather than scattering their
dereferences across a function or case block. Keep status flags (such as boolean
indicators of valid data) immediately adjacent to their corresponding output
data or size assignments.

**Why.** Scattered writes to output parameters make it difficult to verify that
output state is fully produced and synchronized. Grouping related output
assignments ensures status flags and associated output payloads are updated
together, improving readability and preventing inconsistent output states.

## Examples

Bad:

```cpp
case AOM_CODEC_CX_FRAME_PKT:
  ++stream->frames_out;
  *frame_size = pkt->data.frame.sz;

  update_rate_histogram(stream->rate_hist, cfg, pkt);
  write_frame_data(stream, pkt);
  stream->nbytes += pkt->data.raw.sz;

  *got_data = 1;
  break;
```

Good:

```cpp
case AOM_CODEC_CX_FRAME_PKT:
  ++stream->frames_out;

  update_rate_histogram(stream->rate_hist, cfg, pkt);
  write_frame_data(stream, pkt);
  stream->nbytes += pkt->data.raw.sz;

  *got_data = 1;
  *frame_size = pkt->data.frame.sz;
  break;
```
