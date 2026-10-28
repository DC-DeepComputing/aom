---
name: libaom/avoid-changelog-or-historical-notes-in-comments
description: When writing source code comments, avoid recording changelog notes or historical modifications; document the current state, purpose, and invariants instead.
rubric: readability
severity: nit
---

**Rule.** Do not write code comments that describe historical modifications,
diff history, or changelog notes (such as stating that a setting or value was
changed for a specific feature or build). In-source comments must describe the
current state, purpose, or invariants of the code, or be omitted if
self-evident; record change history in commit messages instead.

**Why.** Source comments persist indefinitely in the codebase and become
confusing or obsolete when describing past changes rather than the present
design; version control history and commit messages exist to record historical
modifications.

## Examples

Bad:

```cpp
// Settings changed for realtime only build.
static const AOM_USAGE_REALTIME_CONFIG kRealtimeConfig = {
    AOM_USAGE_REALTIME,  // usage
    10,                  // cpu_used
};
```

Good:

```cpp
// Default encoder configuration for realtime-only builds.
static const AOM_USAGE_REALTIME_CONFIG kRealtimeConfig = {
    AOM_USAGE_REALTIME,  // usage
    10,                  // cpu_used
};
```
