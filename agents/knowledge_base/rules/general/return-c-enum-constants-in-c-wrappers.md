---
name: libaom/return-c-enum-constants-in-c-wrappers
description: When returning enum values from C API or extern C wrapper functions, return C-level enum constants rather than C++ namespaced aliases
rubric: readability
severity: nit
---

**Rule.** In C API and extern "C" wrapper functions declared with C return
types, return C-level enum constants (such as kAom* or aom_*) rather than their
C++ namespaced aliases (such as aom::k*).

**Why.** Returning C-level enum constants matches the declared C function
signature and prevents leaking internal C++ namespace qualifiers into C
interface wrapper implementations.

## Examples

Bad:

```cpp
AomFrameDropDecision av1_ratecontrol_rtc_compute_qp(
    void *controller, const AomAV1FrameParamsRTC *frame_params) {
  if (controller == nullptr || frame_params == nullptr)
    return aom::kFrameDropDecisionOk;
  return reinterpret_cast<aom::AV1RateControlRTC *>(controller)
      ->ComputeQP(*frame_params);
}
```

Good:

```cpp
AomFrameDropDecision av1_ratecontrol_rtc_compute_qp(
    void *controller, const AomAV1FrameParamsRTC *frame_params) {
  if (controller == nullptr || frame_params == nullptr)
    return kAomFrameDropDecisionOk;
  return reinterpret_cast<aom::AV1RateControlRTC *>(controller)
      ->ComputeQP(*frame_params);
}
```
