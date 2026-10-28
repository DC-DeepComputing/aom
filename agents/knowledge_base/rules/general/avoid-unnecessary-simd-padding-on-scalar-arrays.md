---
name: libaom/avoid-unnecessary-simd-padding-on-scalar-arrays
description: When defining array dimensions or allocating buffers, do not apply SIMD alignment padding unless elements are accessed via vectorized operations
rubric: safety
severity: should_fix
---

**Rule.** Do not apply SIMD or vector alignment padding macros (such as
ALIGN_POWER_OF_TWO or ALIGN_MULTIPLE_OF_FOUR) to array dimensions, buffer
allocations, or struct capacities when elements are accessed scalar-wise or
element-by-element. Reserve alignment padding strictly for arrays and buffers
whose elements are loaded, stored, or processed in vectorized SIMD chunks.

**Why.** Adding SIMD alignment padding to scalar-accessed arrays wastes memory
in structs and buffers, increases cache footprint, and obscures the actual
logical capacity and indexing boundaries of the data structure.

## Examples

Bad:

```cpp
// Anti-pattern: Padding scalar sparse features array to a multiple of 4
MODEL_ARRAYS(intra_y_mode, ALIGN_MULTIPLE_OF_FOUR(EM_NUM_Y_SPARSE_FEATURES),
             ALIGN_MULTIPLE_OF_FOUR(EM_NUM_Y_DENSE_FEATURES),
             ALIGN_MULTIPLE_OF_FOUR(EM_Y_OUTPUT_SIZE));

// Anti-pattern: Aligning scalar buffer dimension
uint8_t scalar_lut[ALIGN_POWER_OF_TWO(MAX_ENTRIES, 2)];
```

Good:

```cpp
// Compliant: Use exact capacity for scalar array; pad only vectorized arrays
MODEL_ARRAYS(intra_y_mode, EM_NUM_Y_SPARSE_FEATURES,
             ALIGN_MULTIPLE_OF_FOUR(EM_NUM_Y_DENSE_FEATURES),
             ALIGN_MULTIPLE_OF_FOUR(EM_Y_OUTPUT_SIZE));

// Compliant: Sized to exact capacity when processed element-by-element
uint8_t scalar_lut[MAX_ENTRIES];
```
