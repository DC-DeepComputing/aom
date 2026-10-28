---
name: libaom/encode-units-in-identifier-names
description: When declaring variables, struct fields, or parameters representing time, bitrate, or physical quantities, encode the unit suffix in the identifier name
rubric: readability
severity: nit
---

**Rule.** When defining variables, struct members, function parameters, or
constants that represent time durations, rates, memory sizes, or physical
quantities, encode the unit suffix directly in the identifier name (such as
`_ms`, `_us`, `_sec`, `_fps`, `_pct`, `_kbps`, `_bytes`) or explicitly document
the unit scale in a declaration comment. Avoid ambiguous or unitless names (such
as bare `_time` or `_interval`) when a specific physical unit or time scale is
required.

**Why.** Numeric quantities representing durations, bitrates, or percentages are
ambiguous without explicit units (e.g., distinguishing seconds from milliseconds
or frame counts). Explicit unit suffixes or comments prevent scale conversion
mistakes, off-by-factor bugs, and configuration mismatches across callers and
subsystems.

## Examples

Bad:

```cpp
struct AV1RateControlRtcConfig {
  int frame_drop_thresh;
  int max_consec_drop_time;  // Ambiguous unit (ms? seconds? frames?)
  double framerate;
  int target_bandwidth;      // Ambiguous (bps? kbps?)
};
```

Good:

```cpp
struct AV1RateControlRtcConfig {
  int frame_drop_thresh;
  int max_consec_drop_ms;    // Explicit unit suffix
  double framerate;
  int target_bandwidth_kbps; // Explicit unit suffix
};
```
