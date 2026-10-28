---
name: libaom/validate-c-api-pointer-parameters-against-null
description: When implementing C API or extern "C" wrapper functions, validate all incoming pointer arguments against NULL or nullptr before dereferencing or casting.
rubric: safety
severity: blocking
---

**Rule.** In C API entry points and `extern "C"` wrapper functions, validate all
incoming pointer parameters (such as opaque handles and configuration struct
pointers) against `nullptr` or `NULL` before dereferencing or casting them.
Return an appropriate failure code, `false`, or default value immediately when
any required pointer parameter is null.

**Why.** Functions exposed via C APIs or `extern "C"` wrappers cannot rely on
C++ non-null references and can receive null pointers from external or
foreign-language callers. Dereferencing unvalidated pointers results in null
pointer dereferences, crashes, and undefined behavior.

## Examples

Bad:

```cpp
bool av1_ratecontrol_rtc_update(
    AomAV1RateControlRTC *controller,
    const struct AV1RateControlRtcConfig *rc_cfg) {
  return reinterpret_cast<aom::AV1RateControlRTC *>(controller)
      ->UpdateRateControl(*rc_cfg);
}
```

Good:

```cpp
bool av1_ratecontrol_rtc_update(
    AomAV1RateControlRTC *controller,
    const struct AV1RateControlRtcConfig *rc_cfg) {
  if (controller == nullptr || rc_cfg == nullptr) return false;
  return reinterpret_cast<aom::AV1RateControlRTC *>(controller)
      ->UpdateRateControl(*rc_cfg);
}
```
