---
name: libaom/use-opaque-struct-pointers-for-c-handles
description: When declaring C or C-compatible header APIs with opaque handles or context objects, use forward-declared opaque struct pointers instead of void*
rubric: readability
severity: should_fix
---

**Rule.** In C and C-compatible header APIs, define and use forward-declared
opaque struct pointers (e.g., typedef struct OpaqueType OpaqueType;) instead of
void * for opaque handles and context objects. Expose functions that create,
manipulate, or destroy handles using typed OpaqueType * (or const OpaqueType *)
parameter and return types.

**Why.** Using untyped void * for handle or context pointers bypasses compiler
type checking, allowing callers to accidentally pass incompatible pointer types
without compiler errors or warnings. Forward-declaring an opaque struct
preserves encapsulation by hiding internal implementation details while ensuring
strict compile-time type safety.

## Examples

Bad:

```cpp
// Using untyped void * for opaque handles bypasses type safety
void *av1_ratecontrol_rtc_create(const AomAV1RateControlRtcConfig *rc_cfg);
void av1_ratecontrol_rtc_destroy(void *controller);
bool av1_ratecontrol_rtc_update(void *controller,
                                const AomAV1RateControlRtcConfig *rc_cfg);
```

Good:

```cpp
// Forward-declare an opaque struct type for compile-time type safety
struct AomAV1RateControlRTC;
typedef struct AomAV1RateControlRTC AomAV1RateControlRTC;

AomAV1RateControlRTC *av1_ratecontrol_rtc_create(
    const AomAV1RateControlRtcConfig *rc_cfg);
void av1_ratecontrol_rtc_destroy(AomAV1RateControlRTC *controller);
bool av1_ratecontrol_rtc_update(AomAV1RateControlRTC *controller,
                                const AomAV1RateControlRtcConfig *rc_cfg);
```
