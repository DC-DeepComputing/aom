---
name: libaom/route-diagnostics-to-stderr-not-stdout
description: When adding or modifying logging or instrumentation, remove active debug macros and temporary prints, and route legitimate diagnostic messages to stderr rather than stdout
rubric: readability
severity: should_fix
---

**Rule.** Do not leave active debug macros, verbose instrumentation, statistics
dumping defines (such as `#define OUTPUT_*_STATS`), or temporary debug print
statements enabled in submitted production code; disable or remove them before
submitting. For legitimate diagnostic, informational, or status logging that
must remain, direct output to `stderr` rather than `stdout` to avoid corrupting
encoded bitstream or raw frame data pipelines.

**Why.** Leaving debug macros or verbose instrumentation enabled causes unwanted
output and degrades performance in production builds. Furthermore, printing
diagnostics to stdout corrupts bitstream or raw frame data piped by CLI tools
and encoders, whereas routing diagnostics to stderr keeps data channels clean.

## Examples

Bad:

```cpp
// Active debug macro left enabled in submitted code:
#define OUTPUT_SCR_DET_MODE2_STATS

#ifdef OUTPUT_ENH_SCC_DILATION_STATS
  fprintf(stdout, "\nOriginal values: %d\n", src[0]);
#endif
```

Good:

```cpp
// Disable or remove internal debug instrumentation before submitting:
// #define OUTPUT_SCR_DET_MODE2_STATS

// Temporary prints removed; legitimate diagnostics routed to stderr:
static void log_warning(int frame_idx) {
  fprintf(stderr, "Warning: frame %d dropped\n", frame_idx);
}
```
