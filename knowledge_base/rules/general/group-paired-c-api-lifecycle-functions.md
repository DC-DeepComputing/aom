---
name: libaom/group-paired-c-api-lifecycle-functions
description: When declaring or implementing C API object lifecycle functions, place destruction and cleanup helpers immediately following their corresponding creation or initialization functions
rubric: readability
severity: nit
---

**Rule.** In C and C-compatible (`extern "C"`) API headers and implementation
files, place object destruction, deletion, and cleanup functions (such as
`*_destroy`, `*_delete`, or `*_free`) immediately following their corresponding
creation or initialization functions (such as `*_create` or `*_init`). Do not
separate paired lifecycle management functions with intermediate operational or
accessor methods.

**Why.** Grouping paired creation and destruction functions together keeps
object lifecycle management proximate, making allocation and deallocation easier
to review, audit for leaks, and verify side-by-side.

## Examples

Bad:

```cpp
AomAV1RateControlRTC *av1_ratecontrol_rtc_create(
    const AomAV1RateControlRtcConfig *rc_cfg);
bool av1_ratecontrol_rtc_update(AomAV1RateControlRTC *controller,
                                const AomAV1RateControlRtcConfig *rc_cfg);
int av1_ratecontrol_rtc_get_qp(const AomAV1RateControlRTC *controller);
void av1_ratecontrol_rtc_destroy(AomAV1RateControlRTC *controller);
```

Good:

```cpp
AomAV1RateControlRTC *av1_ratecontrol_rtc_create(
    const AomAV1RateControlRtcConfig *rc_cfg);
void av1_ratecontrol_rtc_destroy(AomAV1RateControlRTC *controller);
bool av1_ratecontrol_rtc_update(AomAV1RateControlRTC *controller,
                                const AomAV1RateControlRtcConfig *rc_cfg);
int av1_ratecontrol_rtc_get_qp(const AomAV1RateControlRTC *controller);
```
