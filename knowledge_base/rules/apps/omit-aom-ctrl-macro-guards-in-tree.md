---
name: libaom/apps/omit-aom-ctrl-macro-guards-in-tree
description: 'When invoking codec controls in in-tree code (apps, examples, tests, tools), call them unconditionally without #ifdef AOM_CTRL_* preprocessor guards'
rubric: readability
severity: should_fix
---

**Rule.** Do not wrap aom_codec_control calls with #ifdef AOM_CTRL_<ID> (or #if
defined(AOM_CTRL_<ID>)) preprocessor guards in in-tree code (such as apps/,
examples/, tools/, or test/). Invoke the codec control unconditionally;
AOM_CTRL_* compile-time feature-check macros are intended solely for external
third-party consumers building against unpinned libaom versions.

**Why.** In-tree applications, tools, and tests build directly against the
repository's own headers where all defined control IDs are guaranteed to be
available. Wrapping control IDs in compile-time macro checks creates unnecessary
preprocessor clutter and redundant code paths.

## Examples

Bad:

```cpp
#ifdef AOM_CTRL_AV1E_GET_TARGET_SEQ_LEVEL_IDX
aom_codec_control(&stream->encoder, AV1E_GET_TARGET_SEQ_LEVEL_IDX,
                  target_levels);
#endif
```

Good:

```cpp
aom_codec_control(&stream->encoder, AV1E_GET_TARGET_SEQ_LEVEL_IDX,
                  target_levels);
```
