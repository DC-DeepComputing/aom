---
name: libaom/remove-obsolete-todo-comments
description: When implementing features, adding test coverage, refactoring, or finalizing heuristics, remove or update obsolete or completed TODO comments
rubric: readability
severity: nit
---

**Rule.** When implementing a feature, bug fix, unit test coverage, code
refactoring, or finalizing heuristic tuning tracked by an existing `TODO`,
`FIXME`, or `XXX` comment, remove the obsolete comment or update it to reflect
any remaining pending work. Do not leave completed or stale `TODO` annotations
in header or source files once the referenced tasks, experiments, or cleanups
are fulfilled.

**Why.** Leaving obsolete or fulfilled `TODO` comments after implementing
changes misleads reviewers and maintainers into believing tasks, tests, or
cleanups remain pending, creating confusion about production readiness and
codebase status.

## Examples

Bad:

```cpp
// Variant 1: Stale TODO left after finalizing heuristic tuning
// TODO(username): Test, tweak and turn on either 1 or 2
sf->inter_sf.inter_mode_rd_model_estimation =
    cpi->oxcf.algo_cfg.sharpness ? 0 : 1;

// Variant 2: Obsolete TODO left after implementing unit tests or data structures
/*!\brief  aom region of interest map
 * TODO(username): create a unit test for ROI map related APIs
 */
typedef struct aom_roi_map {
```

Good:

```cpp
// Variant 1: Removed stale TODO once heuristics are resolved
sf->inter_sf.inter_mode_rd_model_estimation =
    cpi->oxcf.algo_cfg.sharpness ? 0 : 1;

// Variant 2: Clean struct documentation without outdated tracking notes
/*!\brief  aom region of interest map
 */
typedef struct aom_roi_map {
```
