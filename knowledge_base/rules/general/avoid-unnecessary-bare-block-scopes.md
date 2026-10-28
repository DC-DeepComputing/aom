---
name: libaom/avoid-unnecessary-bare-block-scopes
description: Avoid unnecessary bare or nested block scopes ({ ... }) in function and conditional bodies unless required for variable lifetime management, RAII scoping, or name isolation
rubric: readability
severity: nit
---

**Rule.** Do not enclose function or conditional logic in unnecessary bare or
nested compound statement blocks (`{ ... }`), such as superfluous enclosing
braces left behind after refactoring branches into early returns or redundant
inner scopes around statements. Flatten statements to the enclosing function or
conditional scope unless explicit braces are required for variable lifetime
management, RAII scoping, or name isolation.

**Why.** Extraneous bare compound statements and redundant inner block scopes
introduce needless indentation depth and visual noise without providing
functional, scoping, or RAII benefits. Flattening code simplifies control flow
and keeps statements and variable declarations at a clean, consistent
indentation level.

## Examples

Bad:

```cpp
// Variant 1: Leftover bare block scope after early return
if (is_sub8x8_inter) {
  return;
}
{
  InterPredParams inter_pred_params;
  struct buf_2d *const dst_buf = &pd->dst;
  av1_init_inter_params(&inter_pred_params, ...);
}

// Variant 2: Redundant nested compound block inside conditional
if (bestsme == INT_MAX || cpi->sf.mv_sf.intrabc_search_level == 0) {
  int pixelsme = av1_full_pixel_search(...);
  {
    if (pixelsme < bestsme) {
      bestsme = pixelsme;
      best_mv = best_pixel_mv;
    }
  }
}
```

Good:

```cpp
// Variant 1: Flattened statements in function body
if (is_sub8x8_inter) {
  return;
}
InterPredParams inter_pred_params;
struct buf_2d *const dst_buf = &pd->dst;
av1_init_inter_params(&inter_pred_params, ...);

// Variant 2: Direct statements without redundant inner block
if (bestsme == INT_MAX || cpi->sf.mv_sf.intrabc_search_level == 0) {
  const int pixelsme = av1_full_pixel_search(...);
  if (pixelsme < bestsme) {
    bestsme = pixelsme;
    best_mv = best_pixel_mv;
  }
}
```
