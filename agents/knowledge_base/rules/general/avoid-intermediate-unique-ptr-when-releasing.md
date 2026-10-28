---
name: libaom/avoid-intermediate-unique-ptr-when-releasing
description: When transferring raw pointer ownership from a factory returning std::unique_ptr, invoke .release() directly on the temporary rather than declaring an intermediate variable
rubric: readability
severity: nit
---

**Rule.** When releasing ownership of a dynamically allocated object returned by
a factory function (such as std::make_unique or a class Create() method), invoke
.release() directly on the temporary rather than declaring an intermediate
std::unique_ptr local variable. Avoid declaring a named std::unique_ptr variable
solely to call .release() on the following line.

**Why.** Declaring an intermediate std::unique_ptr variable solely to
immediately call .release() introduces unnecessary local state and boilerplate
without providing lifetime or safety benefits. Chaining .release() directly onto
the factory expression makes the ownership transfer concise and
self-documenting.

## Examples

Bad:

```cpp
void *av1_ratecontrol_rtc_create(const AV1RateControlRtcConfig *rc_cfg) {
  std::unique_ptr<aom::AV1RateControlRTC> controller;
  controller = aom::AV1RateControlRTC::Create(*rc_cfg);
  return controller.release();
}
```

Good:

```cpp
void *av1_ratecontrol_rtc_create(const AV1RateControlRtcConfig *rc_cfg) {
  return aom::AV1RateControlRTC::Create(*rc_cfg).release();
}
```
