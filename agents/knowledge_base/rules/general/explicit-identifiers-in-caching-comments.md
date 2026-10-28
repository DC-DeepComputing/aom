---
name: libaom/explicit-identifiers-in-caching-comments
description: When documenting caching, buffering, or fallback mechanisms in comments, explicitly name specific struct members and variables instead of vague terms like 'the cache'
rubric: readability
severity: nit
---

**Rule.** When writing comments explaining caching, buffering, fallback
mechanisms, or state sharing across levels, explicitly name the concrete struct
members and variables (e.g., AV1_COMP::mv_search_params vs.
MACROBLOCK::search_site_cfg_buf). Avoid ambiguous, generic phrases like 'the
cache' or 'the buffer' that obscure which tier or struct holds the data.

**Why.** Generic terms like 'the cache' or 'the buffer' create ambiguity when
multiple storage tiers or scopes exist (such as compressor-level shared caches
versus thread-local temporary buffers). Explicitly naming the struct members and
variables clarifies data ownership, scope, and concurrency expectations for
future readers.

## Examples

Bad:

```cpp
// If we cannot fetch directly from the cache, then we use ss_cfg_buf as the
// backup. We assume that ss_cfg_buf is safe to write to.
if (ref_stride != cached_stride) {
  refresh_config(x->ss_cfg_buf, ref_stride);
}
```

Good:

```cpp
// AV1_COMP::mv_search_params.search_site_cfg is a compressor-level cache
// shared across threads. If it does not contain the matching stride, fall back
// to the thread-local MACROBLOCK::search_site_cfg_buf.
if (ref_stride != cpi->mv_search_params.search_site_cfg[SS_CFG_SRC]->stride) {
  refresh_config(x->search_site_cfg_buf, ref_stride);
}
```
