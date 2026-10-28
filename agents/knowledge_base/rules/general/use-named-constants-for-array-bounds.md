---
name: libaom/use-named-constants-for-array-bounds
description: When specifying array capacities, buffer dimensions, pointer offsets, partition strides, loop bounds, CDF tables, or precision scales, use named constants or derived expressions instead of magic numbers
rubric: readability
severity: should_fix
---

**Rule.** Do not use hardcoded magic numbers or raw numeric literals for fixed
array dimensions, buffer capacities, CDF probability tables (e.g., in
CDF_SIZE(...)), struct member bounds, loop iteration limits, search ranges,
sorting counts (qsort), pointer stepping offsets, partition strides, or
precision-dependent scale and rounding offsets. Define and use descriptive named
constants (#define macros such as MAX_*, AOM_RC_MAX_REF_FRAMES,
MFQE_N_GRID_SEARCH, or block offset helpers like SECOND_32_BLK; enum constants;
or const/constexpr variables like kWidth), parameter variables, or explicit
derived expressions (such as TOTAL_SUBBLOCKS, TOTAL_ELEMENTS / NUM_PARTITIONS,
SUBBLOCKS_IN_SB / 8, or (1 << (DIST_PRECISION_BITS - 1))):
1. Array and struct bounds: Use named constants for dimensions, CDF tables, and struct member arrays, documenting capacity assumptions where appropriate.
2. Loops and search ranges: Consistently use established constants (such as MAX_MB_PLANE) or parameterized variables across all loop bounds.
3. Pointer stepping and buffer strides: Use defined block offset helper macros (such as SECOND_32_BLK) rather than open-coded multiplier offsets.
4. Partition and indexing steps: Express derived array indices, subblock strides, and partition sizes using named constants or explicit formulas instead of collapsing arithmetic into raw literals.
5. Precision scales and offsets: Express rounding offsets, weights, and scaling factors in terms of defined precision macros (such as (1 << (PRECISION_BITS - 1))) rather than hardcoding raw numbers.

**Why.** Hardcoded numeric literals and collapsed multiplier arithmetic obscure
semantic meaning, capacity assumptions, units, and derivation of buffer extents,
partition strides, and precision-dependent offsets. Centralizing dimensions,
offsets, and scales in defined constants or explicit formulas prevents
out-of-bounds indexing and sizing mismatches across allocations, loops, and
sorting calls, ensures consistency across multi-block routines, makes
algorithmic intent self-documenting, and prevents off-by-one or mismatched bound
bugs when codec limits, symbol counts, precision, or padding change.

## Examples

Bad:

```cpp
// Variant 1: Magic numbers for array dimensions, CDF tables, struct bounds, and qsort.
float features[54];
aom_cdf_prob rotation_degree_cdf[CDF_SIZE(15)];
struct aom_ext_ref_frame_info { int ref_frame_coding_indexes[3]; };
qsort(variances, 64, sizeof(variances[0]), compare_uint);

// Variant 2: Hardcoded loop bounds, search ranges, and pointer stepping offsets.
for (int i = num_planes; i < 3; ++i) xd->plane[i].subsampling_x = 0;
for (int j = -30; j <= 30; ++j) search_coord(j * step_theta);
load_convolve_round_8tap_32_avx2(src_ptr + 1 * 32, coeffs, filt, dst + 1 * 32);

// Variant 3: Hardcoded partition indexing strides and precision offsets.
const unsigned int variance = variances[octile * 8 - 1];
*fwd_offset = 8;
*bck_offset = 8;
```

Good:

```cpp
// Variant 1: Named constants for dimensions, CDF sizes, struct bounds, and qsort.
float features[UV_INTRA_MODE_FEATURES];
aom_cdf_prob rotation_degree_cdf[CDF_SIZE(ROTATION_COUNT)];
struct aom_ext_ref_frame_info { int ref_frame_coding_indexes[AOM_RC_MAX_REF_FRAMES]; };
qsort(variances, TOTAL_SUBBLOCKS, sizeof(variances[0]), compare_uint);

// Variant 2: Defined named constants, search parameters, and block offset macros.
for (int i = num_planes; i < MAX_MB_PLANE; ++i) xd->plane[i].subsampling_x = 0;
for (int j = -search_range; j <= search_range; ++j) search_coord(j * step_theta);
load_convolve_round_8tap_32_avx2(src_ptr + SECOND_32_BLK, coeffs, filt, dst + SECOND_32_BLK);

// Variant 3: Derived formulas/macros for partition strides and precision offsets.
#define SUBBLOCKS_IN_OCTILE (SUBBLOCKS_IN_SB / 8)
const unsigned int variance = variances[octile * SUBBLOCKS_IN_OCTILE - 1];
*fwd_offset = 1 << (DIST_PRECISION_BITS - 1);
*bck_offset = 1 << (DIST_PRECISION_BITS - 1);
```
