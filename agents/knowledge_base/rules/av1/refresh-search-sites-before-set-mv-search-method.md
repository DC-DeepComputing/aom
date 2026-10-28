---
name: libaom/av1/refresh-search-sites-before-set-mv-search-method
description: When refreshing motion search site configurations before calling av1_set_mv_search_method, update the local search_sites pointer rather than assigning directly to ms_params->search_sites
rubric: correctness
severity: should_fix
---

**Rule.** Do not assign directly to ms_params->search_sites when refreshing
motion search site configurations prior to calling av1_set_mv_search_method.
Instead, update the local search_sites pointer so that av1_set_mv_search_method
initializes ms_params->search_sites with the refreshed configuration.

**Why.** av1_set_mv_search_method unconditionally overwrites
ms_params->search_sites from the passed-in search_sites array. Assigning
directly to ms_params->search_sites prior to that call is immediately
overwritten, leaving motion search using an unadjusted search site stride.

## Examples

Bad:

```cpp
if (ref_stride != search_sites[search_method].stride) {
  ms_params->search_sites = av1_refresh_search_site_config(
      x->search_site_cfg_buf, search_method, ref_stride);
}
av1_set_mv_search_method(ms_params, search_sites, search_method);
```

Good:

```cpp
if (ref_stride != search_sites[search_method].stride) {
  av1_refresh_search_site_config(x->search_site_cfg_buf, search_method,
                                 ref_stride);
  search_sites = x->search_site_cfg_buf;
}
av1_set_mv_search_method(ms_params, search_sites, search_method);
```
