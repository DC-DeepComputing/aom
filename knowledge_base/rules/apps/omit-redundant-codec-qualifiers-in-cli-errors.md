---
name: libaom/apps/omit-redundant-codec-qualifiers-in-cli-errors
description: When writing CLI and application error messages in tools or examples, name the specific configuration directly without redundant qualifiers like 'codec'
rubric: readability
severity: nit
---

**Rule.** In CLI utilities, tools, and sample applications (such as in calls to
ctx_exit_on_error(), die_codec(), or die()), name the specific configuration
option, control, or property directly. Do not include redundant qualifiers like
'codec' (e.g., use 'Failed to set color range' rather than 'Failed to set codec
color range').

**Why.** In encoder and decoder CLI utilities where the codec context is already
established, omitting redundant qualifiers like 'codec' keeps user-facing error
messages concise and consistent with neighboring error messages across tools.

## Examples

Bad:

```cpp
ctx_exit_on_error(&stream->encoder, "Failed to set codec color range");
die_codec(&codec, "Failed to set codec cpu-used");
```

Good:

```cpp
ctx_exit_on_error(&stream->encoder, "Failed to set color range");
die_codec(&codec, "Failed to set cpu-used");
```
