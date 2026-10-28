---
name: libaom/do-not-return-boolean-literals-from-non-boolean-functions
description: When returning from functions declared with non-boolean return types such as int, return appropriate numeric literals or error codes rather than boolean true or false
rubric: readability
severity: should_fix
---

**Rule.** Do not return boolean literals ('false' or 'true') from functions
declared with non-boolean return types (such as 'int', enum, or pointer types).
Return an appropriate literal or constant matching the declared return type
(such as 0, an error code, or nullptr) instead of relying on implicit boolean
conversion.

**Why.** Returning boolean literals from integer or non-boolean functions relies
on implicit type conversion to 0 or 1, which obscures the function's actual
return type and misleads readers about return conventions.

## Examples

Bad:

```cpp
int av1_ratecontrol_rtc_get_qp(const void *controller) {
  if (controller == nullptr) return false;
  return reinterpret_cast<const aom::AV1RateControlRTC *>(controller)->GetQP();
}
```

Good:

```cpp
int av1_ratecontrol_rtc_get_qp(const void *controller) {
  if (controller == nullptr) return 0;
  return reinterpret_cast<const aom::AV1RateControlRTC *>(controller)->GetQP();
}
```
