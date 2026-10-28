---
name: libaom/av1/do-not-modify-spec-tables-for-test-inputs
description: When unit testing DSP or SIMD routines across block sizes, do not modify codec specification lookup tables; construct standalone test buffers directly in unit tests
rubric: correctness
severity: should_fix
---

**Rule.** Do not modify codec specification lookup tables (such as
av1_wedge_params_lookup) to accommodate unit test inputs or to support block
sizes and modes not permitted by the bitstream specification. When unit testing
DSP or SIMD routines for dimensions beyond specification limits, construct
synthetic or standalone test input buffers directly within unit tests in test/.

**Why.** Codec lookup tables in common codec modules define normative decoder
behavior and must strictly conform to bitstream specification constraints.
Modifying production tables to satisfy unit test setups can introduce invalid
bitstream parsing or corrupted decoder states; unit tests should supply their
own test vectors without altering production codec definitions.

## Examples

Bad:

```cpp
// In av1/common/reconinter.c: Modifying spec tables for unit test block sizes:
const wedge_params_type av1_wedge_params_lookup[BLOCK_SIZES_ALL] = {
  ...
  { MAX_WEDGE_TYPES, wedge_codebook_16_heqw, wedge_signflip_lookup[BLOCK_64X64],
    wedge_masks[BLOCK_64X64] },
};
```

Good:

```cpp
// In test/comp_mask_variance_test.cc: Construct test buffers directly in tests:
uint8_t mask[2 * MAX_WEDGE_TYPES * 4 * MAX_WEDGE_SQUARE];
diffwtd_mask(mask, inv, 38, pred_, w, ref_, MAX_SB_SIZE, h, w);
aom_comp_mask_upsampled_pred_c(..., mask, ...);
```
