---
name: libaom/avoid-unnecessary-single-use-intermediate-variables
description: Inline concise expressions, field lookups, casts, or canonical constants and macros directly at point of use instead of declaring redundant single-use intermediate variables or local aliases
rubric: readability
severity: nit
---

**Rule.** Do not declare redundant single-use intermediate or local alias
variables when named constants, enums, preprocessor macros, field lookups, type
casts, or concise expressions can be evaluated or referenced directly at the
point of use. Inline the constant or expression directly into consuming
statements, conditions, ternary operators, SIMD intrinsics (adding an explicit
type cast if required), or function arguments; return cast or computed values
directly (e.g., `return (uint8_t)val;`); and reassign destination variables
directly with function results rather than storing them in an intermediate
variable first. Only declare intermediate named variables when necessary to
break down complex multi-line calculations, clarify non-obvious logic, prevent
repeated expensive evaluations, or manage error cleanup.

**Why.** Declaring redundant local aliases for existing named constants/macros
or single-use intermediate variables for simple lookups, casts, returns, and
reassignments introduces visual noise and boilerplate without improving
readability, type safety, or performance. Using canonical constants directly and
inlining simple expressions clarifies data provenance and simplifies variable
lifetime tracking.

## Examples

Bad:

```cpp
// Bad: Redundant local alias for named constant / macro
const int max_tile_size = MAX_TILE_AREA;
if (level_stats->max_tile_size > max_tile_size) {
  fail_id = TILE_TOO_LARGE;
  break;
}

// Bad: Redundant local alias before SIMD intrinsic
const int32_t scale = NewInvSqrt2;
out_low = vmull_n_s16(vget_low_s16(input[z]), scale);

// Bad: Single-use intermediate variable for struct lookup
const int referenced = cpi->ppi->rtc_ref.reference[ref_frame - 1];
const int map_idx = referenced
                        ? get_ref_frame_map_idx(cm, ref_frame)
                        : first_ref_map_idx;

// Bad: Temporary variable solely to reassign or return
const int adjusted_qindex = av1_adjust_q_from_delta_q_res(
    delta_q_res, xd->current_base_qindex, current_qindex);
current_qindex = adjusted_qindex;

uint8_t ret = (uint8_t)pixel;
return ret;
```

Good:

```cpp
// Good: Reference named constant directly in conditional check
if (level_stats->max_tile_size > MAX_TILE_AREA) {
  fail_id = TILE_TOO_LARGE;
  break;
}

// Good: Pass constant directly into intrinsic (with explicit cast if required)
out_low = vmull_n_s16(vget_low_s16(input[z]), (int16_t)NewInvSqrt2);

// Good: Inline concise field lookup directly into expression
const int map_idx = cpi->ppi->rtc_ref.reference[ref_frame - 1]
                        ? get_ref_frame_map_idx(cm, ref_frame)
                        : first_ref_map_idx;

// Good: Reassign destination variable directly and return cast directly
current_qindex = av1_adjust_q_from_delta_q_res(
    delta_q_res, xd->current_base_qindex, current_qindex);

return (uint8_t)pixel;
```
