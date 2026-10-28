---
name: libaom/av1/document-quantization-delta-qindex-offsets
description: When documenting quantization parameter structs with plane or DC/AC delta fields, clarify that deltas represent offsets relative to base_qindex and note implicit zero deltas
rubric: readability
severity: nit
---

**Rule.** When defining or documenting quantization parameter structs, clarify
in member comments that plane and DC/AC delta fields (such as y_dc_delta_q,
u_dc_delta_q, v_dc_delta_q, u_ac_delta_q, v_ac_delta_q) represent offsets added
to base_qindex. Explicitly note when a plane or coefficient delta is omitted
because it is implicitly zero (such as the Y plane AC delta).

**Why.** Quantization indices for individual planes and frequency coefficients
are computed by adding plane-specific delta offsets to base_qindex. Documenting
the relative offset relationship and implicit zero baselines clarifies how each
coefficient's effective qindex is derived and why explicit delta fields (like
y_ac_delta_q) are omitted.

## Examples

Bad:

```cpp
struct CommonQuantParams {
  int base_qindex;
  int y_dc_delta_q;
  int u_dc_delta_q;
  int v_dc_delta_q;
  int u_ac_delta_q;
  int v_ac_delta_q;
};
```

Good:

```cpp
struct CommonQuantParams {
  // Base qindex of the frame in the range 0 to 255.
  int base_qindex;

  // Delta of qindex (from base_qindex) for Y plane DC coefficient.
  // Note: y_ac_delta_q is implicitly 0.
  int y_dc_delta_q;
  int u_dc_delta_q;
  int v_dc_delta_q;
  int u_ac_delta_q;
  int v_ac_delta_q;
};
```
