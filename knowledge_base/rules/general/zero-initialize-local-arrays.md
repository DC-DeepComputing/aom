---
name: libaom/zero-initialize-local-arrays
description: When declaring local stack arrays whose elements are selectively populated, accumulated into, or read before full initialization, zero-initialize them at declaration (e.g., = { 0 })
rubric: correctness
severity: should_fix
---

**Rule.** Zero-initialize local stack arrays upon declaration (e.g., using = { 0
} or memset) when elements may be selectively written, accumulated into, or read
before all indices are explicitly populated. Do not leave local array
declarations uninitialized when subsequent logic assumes a default zero base.

**Why.** In C, automatic local variables and arrays have indeterminate stack
values when not explicitly initialized. Accumulating into (+=) or reading from
array indices that were not written on every control-flow path accesses
uninitialized stack memory, causing undefined behavior, non-deterministic bugs,
and corrupted calculations.

## Examples

Bad:

```cpp
int channels_per_branch[CNN_MAX_BRANCHES];
for (int i = 0; i < cnn_config->num_layers; ++i) {
  find_cnn_out_channels(&cnn_config->layer_config[i], channels_per_branch);
}
```

Good:

```cpp
int channels_per_branch[CNN_MAX_BRANCHES] = { 0 };
for (int i = 0; i < cnn_config->num_layers; ++i) {
  find_cnn_out_channels(&cnn_config->layer_config[i], channels_per_branch);
}
```
