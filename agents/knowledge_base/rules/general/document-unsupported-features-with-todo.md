---
name: libaom/document-unsupported-features-with-todo
description: When asserting or guarding against unsupported modes or algorithm configurations, document why the feature is unsupported and add an attributed TODO tracking future support
rubric: readability
severity: nit
---

**Rule.** When adding an assertion or runtime guard disabling an unsupported
mode, format, or algorithm configuration, document why it is currently
unsupported in an explanatory comment and add an attributed TODO tracking future
implementation, benchmarking, or evaluation. State the technical reason for the
limitation (such as missing helper variants or unverified performance) rather
than only asserting failure.

**Why.** Bare assertions without explanatory comments leave future maintainers
unaware of why a feature was omitted and what work is required to enable it. An
attributed TODO with clear rationale provides context on prerequisites and
tracks future implementation or performance evaluation.

## Examples

Bad:

```cpp
assert(!ms_params->ms_buffers.second_pred &&
       "Mesh search does not support compound mode!");
```

Good:

```cpp
// TODO(username@google.com): Currently exhaustive search calls the single-ref
// version of sad and variance functions. Evaluate performance and add
// compound-reference support when enabled.
assert(!ms_params->ms_buffers.second_pred &&
       "Mesh search does not support compound mode!");
```
