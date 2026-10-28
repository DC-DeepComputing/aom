---
name: libaom/document-hardcoded-config-overrides-and-limitations
description: When hardcoding, clamping, or overriding configuration settings or container formats due to subsystem limitations, add an explanatory comment stating the underlying constraint
rubric: readability
severity: nit
---

**Rule.** When hardcoding, clamping, or overriding configuration parameters,
container formats, or codec settings (such as forcing IVF output or restricting
rate-control options) due to subsystem constraints or temporary limitations, add
an explanatory comment explaining the underlying limitation. Document why the
setting is forced so future maintainers understand the requirement.

**Why.** Overriding configuration options or forcing specific formats without
documentation appears arbitrary and makes code difficult to maintain or debug.
An explanatory comment clarifies the underlying architectural constraint,
prevents accidental breakage, and indicates what needs updating when subsystem
capabilities expand.

## Examples

Bad:

```cpp
if (need_downscale) {
  stream->config.out_fn = stream->config.two_pass_output;
  stream->config.write_ivf = 1;
  stream->config.write_webm = 0;
}
```

Good:

```cpp
if (need_downscale) {
  stream->config.out_fn = stream->config.two_pass_output;
  // Libaom currently only supports the IVF container format for downscaled passes.
  stream->config.write_ivf = 1;
  stream->config.write_webm = 0;
}
```
