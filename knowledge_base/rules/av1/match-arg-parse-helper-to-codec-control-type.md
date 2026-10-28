---
name: libaom/av1/match-arg-parse-helper-to-codec-control-type
description: When parsing configuration options in encoder_set_option, use the arg_parse helper matching the config field type and its AOM_CTRL_USE_TYPE declaration
rubric: correctness
severity: should_fix
---

**Rule.** When parsing key-value encoder configuration options in
`encoder_set_option`, use the `arg_parse_*_helper` function that matches the
type of the configuration field and its corresponding `AOM_CTRL_USE_TYPE`
definition in public headers. Specifically, use `arg_parse_int_helper` for
signed integers and enum fields, `arg_parse_uint_helper` for unsigned integers,
and `arg_parse_enum_helper` or `arg_parse_rational_helper` for enums and
rational numbers.

**Why.** Using a mismatched parsing helper (such as `arg_parse_uint_helper` for
a signed integer or enum field) causes type discrepancies between
`aom_codec_set_option()` and `aom_codec_control()` and risks improper value
validation or truncation.

## Examples

Bad:

```cpp
// extra_cfg.screen_detection_mode is an enum/int (AOM_CTRL_USE_TYPE uses int)
} else if (arg_match_helper(&arg, &g_av1_codec_arg_defs.screen_detection_mode,
                            name, value)) {
  extra_cfg.screen_detection_mode = arg_parse_uint_helper(&arg, err_string);
}
```

Good:

```cpp
// Use arg_parse_int_helper to match the int/enum type in AOM_CTRL_USE_TYPE
} else if (arg_match_helper(&arg, &g_av1_codec_arg_defs.screen_detection_mode,
                            name, value)) {
  extra_cfg.screen_detection_mode = arg_parse_int_helper(&arg, err_string);
}
```
