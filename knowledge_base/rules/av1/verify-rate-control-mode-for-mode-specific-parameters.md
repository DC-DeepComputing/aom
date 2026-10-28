---
name: libaom/av1/verify-rate-control-mode-for-mode-specific-parameters
description: When conditioning encoder heuristics on mode-specific rate-control parameters like cq_level, verify active usage mode and rate-control mode first
rubric: correctness
severity: should_fix
---

**Rule.** When conditioning encoder decisions, tool controls, or heuristics on
mode-specific rate-control parameters (such as cpi->oxcf.rc_cfg.cq_level),
verify that the encoder is operating in the corresponding rate-control mode
(cpi->oxcf.rc_cfg.mode == AOM_Q or AOM_CQ) and usage mode (cpi->oxcf.mode ==
ALLINTRA). Do not inspect cq_level or other mode-specific settings
unconditionally, as they are inactive during VBR, CBR, or inter-frame encoding.

**Why.** Configuration fields such as cq_level are only meaningful when specific
rate-control modes (like AOM_Q or AOM_CQ) and usage modes (like ALLINTRA) govern
quantization. In other modes (such as CBR, VBR, or inter encoding), cq_level may
retain default or arbitrary values without governing frame quality, causing
incorrect heuristic decisions.

## Examples

Bad:

```cpp
// Checking cq_level without verifying active usage and rate-control modes
if ((cdef_control == CDEF_REFERENCE &&
     cpi->ppi->rtc_ref.non_reference_frame) ||
    (cdef_control == CDEF_ADAPTIVE && cpi->oxcf.rc_cfg.cq_level < 100)) {
  av1_cdef_frame(cpi, &cdef_data);
}
```

Good:

```cpp
// Explicitly verifying usage mode ALLINTRA and rate-control mode AOM_Q / AOM_CQ
const bool is_cq_active =
    cpi->oxcf.mode == ALLINTRA &&
    (cpi->oxcf.rc_cfg.mode == AOM_Q || cpi->oxcf.rc_cfg.mode == AOM_CQ);
if ((cdef_control == CDEF_REFERENCE &&
     cpi->ppi->rtc_ref.non_reference_frame) ||
    (cdef_control == CDEF_ADAPTIVE && is_cq_active &&
     cpi->oxcf.rc_cfg.cq_level < 100)) {
  av1_cdef_frame(cpi, &cdef_data);
}
```
