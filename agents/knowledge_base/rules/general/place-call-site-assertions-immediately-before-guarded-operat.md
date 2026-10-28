---
name: libaom/place-call-site-assertions-immediately-before-guarded-operat
description: When asserting preconditions or parameter constraints for a specific function call, place assertions immediately preceding the guarded call site rather than at enclosing function entry
rubric: readability
severity: nit
---

**Rule.** When an assertion validates arguments, parameter constraints, or
preconditions specific to a downstream function call or intermediate
computation, place the assert() statement immediately preceding that call site
or operation rather than hoisting it to the top of the enclosing function. Keep
assertions proximate to the operations they protect unless they enforce global
function-level entry contracts.

**Why.** Placing call-specific assertions immediately adjacent to the guarded
call site clarifies what operation the check protects, prevents confusion with
function-level entry preconditions, and ensures maintainers see the constraint
when modifying or moving the call.

## Examples

Bad:

```cpp
// Bad: Assertion for a downstream call placed far above at function entry
void compute_frame_costs(AV1_COMP *cpi) {
  assert(cpi->oxcf.tune_cfg.tuning != AOM_TUNE_SSIMULACRA2 &&
         cpi->oxcf.mode != ALLINTRA);

  // ... dozens of lines of intermediate setup and calculations ...

  int rdmult = av1_compute_rd_mult(
      base_qindex, cm->seq_params->bit_depth,
      cpi->ppi->gf_group.update_type[cpi->gf_frame_index], layer_depth,
      boost_index, frame_type, cpi->oxcf.q_cfg.use_fixed_qp_offsets,
      is_stat_consumption_stage(cpi), cpi->oxcf.tune_cfg.tuning);
  // ...
}
```

Good:

```cpp
// Good: Assertion placed immediately preceding the call site it guards
void compute_frame_costs(AV1_COMP *cpi) {
  // ... intermediate setup and calculations ...

  assert(cpi->oxcf.tune_cfg.tuning != AOM_TUNE_SSIMULACRA2 &&
         cpi->oxcf.mode != ALLINTRA);
  int rdmult = av1_compute_rd_mult(
      base_qindex, cm->seq_params->bit_depth,
      cpi->ppi->gf_group.update_type[cpi->gf_frame_index], layer_depth,
      boost_index, frame_type, cpi->oxcf.q_cfg.use_fixed_qp_offsets,
      is_stat_consumption_stage(cpi), cpi->oxcf.tune_cfg.tuning);
  // ...
}
```
