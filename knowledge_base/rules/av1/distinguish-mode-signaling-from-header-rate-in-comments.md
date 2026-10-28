---
name: libaom/av1/distinguish-mode-signaling-from-header-rate-in-comments
description: When documenting rate-distortion cost calculations or pruning heuristics, refer explicitly to mode or parameter signaling rate rather than ambiguous header RD cost
rubric: readability
severity: nit
---

**Rule.** In comments explaining rate-distortion optimization (RDO), mode
pruning, or rate overhead calculations, refer explicitly to 'mode signaling
rate', 'parameter signaling rate', or 'rate needed to signal the
mode/parameters' rather than 'header RD cost' or 'header rate'. Reserve header
terminology for sequence-, frame-, tile-, or OBU-level headers.

**Why.** In video codecs, 'header' typically refers to high-level bitstream
headers (such as sequence, frame, or tile headers). Using 'header RD cost' or
'header rate' to describe block-level syntax or mode signaling overhead creates
ambiguity for readers and maintainers.

## Examples

Bad:

```cpp
// Skip the CfL mode evaluation if its header RD cost (derived using alpha
// and mode signaling rates) exceeds the ref_best_rd.
if (RDCOST(x->rdmult, rate_overhead, 0) > ref_best_rd) {
  return;
}
```

Good:

```cpp
// Skip the CfL mode evaluation if the RD cost derived using rate needed to
// signal the CfL mode and alpha parameter exceeds the ref_best_rd.
if (RDCOST(x->rdmult, rate_overhead, 0) > ref_best_rd) {
  return;
}
```
