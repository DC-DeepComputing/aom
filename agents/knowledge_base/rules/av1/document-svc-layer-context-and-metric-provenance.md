---
name: libaom/av1/document-svc-layer-context-and-metric-provenance
description: When indexing SVC layer contexts like base layer TL0 to constrain rate control parameters across layers, document the target layer and metric provenance
rubric: readability
severity: nit
---

**Rule.** When indexing an SVC layer context (such as referencing base layer TL0
via LAYER_IDS_TO_IDX(0, 0, ...) or svc->layer_context[...]) to constrain rate
control parameters across layers, add comments explicitly identifying the target
layer and explaining which frame statistics (e.g., avg_frame_bandwidth, last_q)
are being evaluated.

**Why.** In multi-layer SVC configurations with multiple temporal or spatial
layers, cross-layer rate control logic can be ambiguous regarding whether bounds
are derived from the base layer (TL0) or the immediate preceding layer.
Documenting the layer identity and metric provenance prevents misunderstandings
during maintenance and tuning.

## Examples

Bad:

```cpp
// Get base temporal layer.
int layer = LAYER_IDS_TO_IDX(0, 0, svc->number_temporal_layers);
LAYER_CONTEXT *lc = &svc->layer_context[layer];
if (rc->avg_frame_bandwidth < lc->rc.avg_frame_bandwidth &&
    q < lc->p_rc.last_q[INTER_FRAME] - 4)
  q = lc->p_rc.last_q[INTER_FRAME] - 4;
```

Good:

```cpp
// Get base temporal layer TL0.
const int layer = LAYER_IDS_TO_IDX(0, 0, svc->number_temporal_layers);
LAYER_CONTEXT *lc = &svc->layer_context[layer];
// lc->rc.avg_frame_bandwidth and lc->p_rc.last_q correspond to the
// last TL0 frame.
if (rc->avg_frame_bandwidth < lc->rc.avg_frame_bandwidth &&
    q < lc->p_rc.last_q[INTER_FRAME] - 4)
  q = lc->p_rc.last_q[INTER_FRAME] - 4;
```
