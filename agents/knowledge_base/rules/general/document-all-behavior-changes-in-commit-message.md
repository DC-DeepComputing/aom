---
name: libaom/document-all-behavior-changes-in-commit-message
description: When writing commit messages for changes affecting defaults or configurations, describe all non-trivial behavior changes across all build modes comprehensively
rubric: readability
severity: should_fix
---

**Rule.** Ensure the commit message accurately and comprehensively describes all
non-trivial configuration and behavior changes in the CL. In particular, do not
limit the description to build-flagged or subsystem-specific paths when the
change also modifies default configurations or behaviors for normal or standard
builds.

**Why.** Commit messages provide essential context for reviewers and future
maintainers tracing behavior regressions. Omitting broad configuration or
default changes while highlighting only build-specific flags misleads readers
about the change's actual blast radius.

## Examples

Bad:

```cpp
rtc: Update default_extra_cfg for CONFIG_REALTIME_ONLY

For CONFIG_REALTIME_ONLY: update the default_extra_cfg
to correspond to the recommended settings used for realtime
coding.
```

Good:

```cpp
rtc: Update default_extra_cfg for CONFIG_REALTIME_ONLY

For CONFIG_REALTIME_ONLY: update the default_extra_cfg
to correspond to the recommended settings used for realtime
coding.

Also, some settings are changed for encoder_usage_cfg,
for AOM_USAGE_REALTIME, whether the build is a normal build
or realtime-only build.
```
