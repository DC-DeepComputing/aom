---
name: libaom/avoid-implicit-boolean-checks-on-multi-valued-options
description: Explicitly compare multi-valued options, mode variables, and enums against named constants or specific numeric values instead of using implicit boolean checks
rubric: readability
severity: should_fix
---

**Rule.** When evaluating multi-valued configuration options (e.g., in `oxcf`,
`q_cfg`, `rc_cfg`, `cfg`), integer mode variables, or enum types in conditionals
(`if`) or ternary expressions (`? :`), explicitly compare them against specific
numeric values or named enumerator constants (e.g., `== 1`, `== 0`, `==
AOM_CR_FULL_RANGE`). Do not use implicit truthiness checks (`if (opt)` or `opt ?
a : b`) or boolean negation (`if (!opt)`). Additionally, when expanding or
converting a boolean flag to a multi-valued enum or integer setting, audit all
existing references across the codebase to ensure all active mode values are
explicitly checked rather than assuming binary truthiness.

**Why.** Configuration options, mode variables, and enums often support multiple
discrete states (such as 0, 1, 2) rather than binary boolean states. Evaluating
non-boolean settings implicitly as booleans relies on arbitrary assumptions
about which enumerator or value corresponds to zero, causes non-zero modes
(e.g., mode 2) to inadvertently trigger branches designed only for mode 1, and
introduces subtle logic bugs and encoder regressions when new modes are added or
code is refactored.

## Examples

Bad:

```cpp
// Implicit boolean checks on multi-valued configuration option and enum
if (cpi->oxcf.q_cfg.use_fixed_qp_offsets && cpi->oxcf.rc_cfg.mode == AOM_Q) {
  setup_fixed_qp_offsets(cpi);
}

if (!use_fixed_qp_offsets && frame_type != KEY_FRAME) {
  av1_set_target_rate(cpi, qp);
}

yuv_constants = (struct YuvConstants *)(color_range ? &kYuvJPEGConstants
                                                    : &kYuvI601Constants);
```

Good:

```cpp
// Explicit comparisons against specific numeric mode values and enum constants
if (cpi->oxcf.q_cfg.use_fixed_qp_offsets == 1 &&
    cpi->oxcf.rc_cfg.mode == AOM_Q) {
  setup_fixed_qp_offsets(cpi);
}

if ((use_fixed_qp_offsets == 0) && frame_type != KEY_FRAME) {
  av1_set_target_rate(cpi, qp);
}

yuv_constants = color_range == AOM_CR_FULL_RANGE ? &kYuvJPEGConstants
                                                 : &kYuvI601Constants;
```
