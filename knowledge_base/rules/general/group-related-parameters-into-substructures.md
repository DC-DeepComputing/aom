---
name: libaom/group-related-parameters-into-substructures
description: Package related configuration parameters into dedicated sub-structures and embed fixed-size sub-structs by value rather than using loose fields or pointers
rubric: design
severity: should_fix
---

**Rule.** Package related sub-feature or component configuration parameters into
dedicated sub-structures rather than adding them as loose, flat fields to parent
structs. Additionally, embed fixed-size parameter and configuration
sub-structures directly by value rather than referencing them through pointers,
signaling feature presence or enablement via field values or zero-initialization
instead of null pointer checks.

**Why.** Grouping related fields into dedicated sub-structures improves
modularity, avoids polluting parent structs with flat members, and simplifies
passing sub-feature configurations as coherent units. Embedding fixed-size
sub-structures by value eliminates heap allocation overhead, pointer
indirection, and memory lifetime management complexity while keeping
configuration objects self-contained and cache-friendly.

## Examples

Bad:

```cpp
// Bad (variant 1): Loose flat fields added directly to parent struct
struct CNN_LAYER_CONFIG {
  int branch;
  BRANCH_COMBINE branch_combine_type;
  int branches_to_combine;
  float *bn_gamma, *bn_beta, *bn_mean, *bn_std;
};

// Bad (variant 2): Fixed-size sub-structure referenced via pointer
struct CNN_LAYER_CONFIG_V2 {
  int in_channels;
  int out_channels;
  struct CNN_BATCHNORM_PARAMS *bn_params;
};
```

Good:

```cpp
// Good: Dedicated sub-structure embedded directly by value
struct CNN_BN_PARAMS {
  float *gamma, *beta, *mean, *std;
};

struct CNN_LAYER_CONFIG {
  int branch;
  BRANCH_COMBINE branch_combine_type;
  int branches_to_combine;
  struct CNN_BN_PARAMS bn_params;  // Embedded by value
};
```
