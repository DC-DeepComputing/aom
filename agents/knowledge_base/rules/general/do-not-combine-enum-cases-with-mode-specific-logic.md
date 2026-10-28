---
name: libaom/do-not-combine-enum-cases-with-mode-specific-logic
description: When branching on enum values or mode flags, do not combine distinct modes in compound conditions if nested logic applies only to a subset
rubric: correctness
severity: should_fix
---

**Rule.** Do not group distinct enum values or mode flags into a single compound
conditional branch (e.g., if (mode == A || mode == B)) when nested statements or
inner conditions apply to only a subset of those modes. Separate distinct modes
into dedicated conditional branches (else-if or switch cases), or explicitly
guard/assert the specific required mode.

**Why.** Merging distinct modes into a shared branch when nested logic only
applies to one mode creates subtle logic bugs, executes invalid operations or
assertions for the other modes, and obscures preconditions for future
maintainers.

## Examples

Bad:

```cpp
} else if (layer_config->branch_copy_type == BRANCH_OUTPUT ||
           layer_config->branch_copy_type == BRANCH_COMBINED) {
  i_width[b] = o_width;
  i_height[b] = o_height;
  i_channel[b] = layer_config->out_channels;
  // Bug: branch combination logic only applies to BRANCH_COMBINED, not BRANCH_OUTPUT
  if (layer_config->branch_combine_type == BRANCH_CAT) {
    process_cat_branch(layer_config);
  }
}
```

Good:

```cpp
} else if (layer_config->branch_copy_type == BRANCH_OUTPUT) {
  i_width[b] = o_width;
  i_height[b] = o_height;
  i_channel[b] = layer_config->out_channels;
} else if (layer_config->branch_copy_type == BRANCH_COMBINED) {
  i_width[b] = o_width;
  i_height[b] = o_height;
  i_channel[b] = layer_config->out_channels;
  if (layer_config->branch_combine_type == BRANCH_CAT) {
    process_cat_branch(layer_config);
  }
}
```
