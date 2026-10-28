---
name: libaom/av1/do-not-gate-auto-config-on-thread-count
description: When configuring automated encoder parameters (such as auto-tiling), invoke setup helpers whenever the feature is enabled rather than gating on multi-threading counts (threads > 1)
rubric: correctness
severity: should_fix
---

**Rule.** When configuring automated encoder features (such as auto-tiling), do
not gate auto-configuration helper calls on multi-threading conditions (e.g.,
max_threads > 1 or threads > 1). Invoke the setup helper whenever the feature is
enabled and let the helper handle single-threaded baseline values.

**Why.** Gating auto-configuration helpers on multi-threading checks prevents
the routine from executing in single-threaded mode (threads == 1), leaving
encoder parameters uninitialized or retaining stale values. Calling the helper
whenever the feature is active ensures valid parameter setup across all thread
configurations.

## Examples

Bad:

```cpp
// Anti-pattern: Gating auto-configuration helper on thread count > 1
if (extra_cfg->auto_tiles && cfg->g_threads > 1) {
  set_auto_tiles(tile_cfg, cfg->g_w, cfg->g_h, cfg->g_threads);
}
```

Good:

```cpp
// Pattern: Invoke helper whenever feature is enabled; helper handles baseline single-thread case
if (extra_cfg->auto_tiles) {
  set_auto_tiles(tile_cfg, cfg->g_w, cfg->g_h, cfg->g_threads);
}
```
