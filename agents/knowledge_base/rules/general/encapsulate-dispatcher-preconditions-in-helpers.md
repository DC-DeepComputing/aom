---
name: libaom/encapsulate-dispatcher-preconditions-in-helpers
description: When delegating module-specific calculations from dispatcher switch cases to dedicated helper functions, encapsulate module-specific preconditions inside each helper function rather than in caller cases.
rubric: design
severity: should_fix
---

**Rule.** When a dispatcher function delegates module-specific logic or
parameter calculations to specialized helper functions via a switch statement,
encapsulate module-specific configuration checks and preconditions inside the
respective helper function rather than in the caller's switch cases. Keep
dispatcher cases uniform and ensure helper functions self-contain and validate
their own execution requirements.

**Why.** Placing module-specific precondition checks in individual dispatcher
cases clutters the dispatching logic, duplicates conditions across multiple
callers, and leaves helper functions vulnerable if called from other contexts
without prior validation.

## Examples

Bad:

```cpp
case MOD_AI:
  if (cpi->oxcf.pass == AOM_RC_ONE_PASS && cpi->oxcf.row_mt) {
    num_mod_workers = compute_num_ai_workers(cpi);
    break;
  } else {
    num_mod_workers = 0;
    break;
  }
```

Good:

```cpp
// Inside compute_num_ai_workers():
if (cpi->oxcf.max_threads <= 1) return 1;
if (!cpi->oxcf.row_mt) return 1;
...

// Inside compute_num_mod_workers():
case MOD_AI:
  if (cpi->oxcf.pass == AOM_RC_ONE_PASS) {
    num_mod_workers = compute_num_ai_workers(cpi);
  } else {
    num_mod_workers = 0;
  }
  break;
```
