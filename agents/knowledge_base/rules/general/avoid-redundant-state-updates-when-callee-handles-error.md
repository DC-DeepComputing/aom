---
name: libaom/avoid-redundant-state-updates-when-callee-handles-error
description: When checking the return value of a stateful helper function on failure, avoid redundantly re-assigning error or validity flags that the callee already updates
rubric: readability
severity: nit
---

**Rule.** When invoking a helper function or method that manages an object or
context's internal validity, status, or error flags upon failure, do not
redundantly re-assign those same state flags at the call site. Rely on the
callee to establish error state and keep call-site failure branches concise.

**Why.** Re-assigning status or validity flags at call sites when the callee
already updates that state on all error return paths introduces redundant state
mutation, adds unnecessary boilerplate, and risks inconsistency if
error-handling logic is updated.

## Examples

Bad:

```cpp
// UpdateRateControl internally sets rc_is_valid_ = false on failure.
if (!UpdateRateControl(rc_cfg)) {
  rc_is_valid_ = false;
  return false;
}
```

Good:

```cpp
// Rely on UpdateRateControl to update internal validity state on failure.
if (!UpdateRateControl(rc_cfg)) {
  return false;
}
```
