---
name: libaom/omit-redundant-out-parameter-initialization
description: When declaring a local variable passed by pointer as an out-parameter to a function that unconditionally populates it, omit redundant default initializers
rubric: readability
severity: nit
---

**Rule.** Do not redundantly initialize a local variable (e.g., `= 0`, `=
false`, or `= { 0 }`) when it is passed by pointer as an out-parameter to a
function that unconditionally sets or overwrites its value. Declare the variable
uninitialized before passing its address to the callee.

**Why.** Initializing a local variable that is immediately and unconditionally
overwritten by a callee out-parameter performs a redundant dead store and
creates visual clutter. Omitting the redundant initialization documents that the
called function is solely responsible for populating the variable.

## Examples

Bad:

```cpp
// Redundant initialization before unconditional out-parameter assignment.
bool use_scaled_ref = 0;
find_predictors(cpi, x, ref_frame, frame_mv, yv12_mb, bsize,
                force_skip_low_temp_var, skip_pred_mv, &use_scaled_ref);
```

Good:

```cpp
// Callee unconditionally assigns to *use_scaled_ref.
bool use_scaled_ref;
find_predictors(cpi, x, ref_frame, frame_mv, yv12_mb, bsize,
                force_skip_low_temp_var, skip_pred_mv, &use_scaled_ref);
```
