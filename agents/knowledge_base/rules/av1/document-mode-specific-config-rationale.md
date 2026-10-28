---
name: libaom/av1/document-mode-specific-config-rationale
description: When overriding default encoder configurations or heuristics for a specific usage mode, document whether the restriction is fundamental or due to testing scope
rubric: readability
severity: nit
---

**Rule.** When overriding default configurations, tuning parameters, or
heuristics conditionally for a specific usage mode (such as AOM_USAGE_ALL_INTRA
or AOM_USAGE_REALTIME), add a comment documenting the rationale. Clarify whether
the parameter override or restriction is fundamentally intrinsic to that mode or
whether it was restricted due to limited testing scope across other modes.

**Why.** Future contributors and maintainers cannot determine from code alone
whether an optimization or default override is fundamentally incompatible with
other usage modes or simply unverified. Documenting the rationale and testing
scope prevents premature generalization or unnecessary duplicate tuning efforts.

## Examples

Bad:

```cpp
// Special handling:
if (priv->cfg.g_usage == AOM_USAGE_ALL_INTRA) {
  priv->extra_cfg.enable_cdef = 0;
  priv->extra_cfg.qm_min = DEFAULT_QM_FIRST_ALLINTRA;
  priv->extra_cfg.qm_max = DEFAULT_QM_LAST_ALLINTRA;
}
```

Good:

```cpp
// Special handling:
if (priv->cfg.g_usage == AOM_USAGE_ALL_INTRA) {
  priv->extra_cfg.enable_cdef = 0;
  // These QM min/max values are beneficial for still images when used
  // with aom_get_qmlevel_allintra(). They may also benefit other usage
  // modes, but are currently restricted pending further testing.
  priv->extra_cfg.qm_min = DEFAULT_QM_FIRST_ALLINTRA;
  priv->extra_cfg.qm_max = DEFAULT_QM_LAST_ALLINTRA;
}
```
