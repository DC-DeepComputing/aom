---
name: libaom/av1/validate-frame-duration-and-update-timestamp-history
description: When computing dynamic framerates from timestamps, verify frame duration is strictly positive before division and maintain timestamp tracking history across early returns
rubric: correctness
severity: blocking
---

**Rule.** When deriving dynamic frame rates or adjusting framerate from frame
timestamps or duration differences (`this_duration = ts_end - ts_start`), always
verify that duration is strictly positive (`this_duration > 0`) before dividing
(`10000000.0 / this_duration`) or invoking `av1_new_framerate`. Guard the
framerate update rather than dividing by zero or assigning an arbitrary default
rate. Additionally, ensure stateful timestamp tracking variables (such as
`prev_ts_start` and `prev_ts_end`) are updated even on zero-duration or
early-return paths so subsequent timestamp deltas remain accurate.

**Why.** Caller-supplied frame durations or timestamps can be zero or identical
across consecutive frames. Dividing by zero produces `inf` or `NaN`, while
assigning arbitrary fallback framerates distorts rate control, GOP pacing, and
buffer levels. Furthermore, failing to update prior timestamp history
(`prev_ts_start`, `prev_ts_end`) on zero-duration frames or early returns
corrupts subsequent frame duration calculations, causing cascading timing
errors.

## Examples

Bad:

```cpp
// Division by zero or assigning arbitrary fallback rate without timestamp update
this_duration = ts_end - ts_start;
cpi->new_framerate = this_duration > 0 ? (10000000.0 / this_duration) : 30.0;
av1_new_framerate(cpi, cpi->new_framerate);
return;
```

Good:

```cpp
this_duration = ts_end - ts_start;
if (this_duration > 0) {
  cpi->new_framerate = 10000000.0 / this_duration;
  av1_new_framerate(cpi, cpi->new_framerate);
}
time_stamps->prev_ts_start = ts_start;
time_stamps->prev_ts_end = ts_end;
return;
```
