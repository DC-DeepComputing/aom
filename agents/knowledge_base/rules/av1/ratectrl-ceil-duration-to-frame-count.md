---
name: libaom/av1/ratectrl-ceil-duration-to-frame-count
description: When converting time-based duration parameters to frame counts in rate control, use ceil consistently across initialization and dynamic update routines.
rubric: correctness
severity: should_fix
---

**Rule.** When converting time durations (such as milliseconds) to frame counts
in rate control configuration, initialization, and dynamic updates, use ceil
(with saturation/casting) consistently rather than round or truncation. Ensure
dynamic update routines (e.g., UpdateRateControl) match initialization
(InitRateControl) so fractional frames do not prematurely truncate the
configured time window.

**Why.** Inconsistent rounding between initialization and dynamic updates causes
discrepant frame-drop behavior upon reconfiguration. Using ceil guarantees that
fractional frame durations do not truncate the configured time window.

## Examples

Bad:

```cpp
// In UpdateRateControl:
if (rc_cfg.max_consec_drop_ms > 0) {
  rc->max_consec_drop =
      (int)round(cpi_->framerate * rc_cfg.max_consec_drop_ms / 1000);
}
```

Good:

```cpp
// In UpdateRateControl:
if (rc_cfg.max_consec_drop_ms > 0) {
  rc->max_consec_drop = saturate_cast_double_to_int(
      ceil(cpi_->framerate * rc_cfg.max_consec_drop_ms / 1000));
}
```
