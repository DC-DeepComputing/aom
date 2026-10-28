---
name: libaom/pass-destination-address-to-out-parameters
description: When passing arguments to out or in-out pointer parameters, pass destination variable, struct field, or array element addresses directly instead of copying through temporary variables
rubric: readability
severity: should_fix
---

**Rule.** When passing arguments to out or in-out pointer parameters whose
results are stored in destination variables, struct members, or array elements,
pass the address of the target storage directly (e.g., &dest_var,
&struct_ptr->member, or &struct_ptr->array[index]) when types match. Do not
declare temporary proxy variables solely to receive out-parameter values and
subsequently copy or assign them to their destination fields.

**Why.** Passing the destination address directly eliminates redundant temporary
proxy variables and unnecessary copy assignments before and after function
invocations. This makes the ultimate output destination immediately evident at
call sites, clarifies data flow, and simplifies local function scopes.

## Examples

Bad:

```cpp
// Bad: Temporary variable used for in-out parameter
size_t curr_frame_size = cpi_data.frame_size;
if (av1_convert_sect5obus_to_annexb(cpi_data.cx_data, cpi_data.cx_data_sz,
                                    &curr_frame_size) != AOM_CODEC_OK) {
  aom_internal_error(&ppi->error, AOM_CODEC_ERROR, NULL);
}
cpi_data.frame_size = curr_frame_size;

// Bad: Temporary variable used for struct array out-parameter
int use_scaled_ref_frame = 0;
find_predictors(cpi, x, ref_frame_iter, search_state->frame_mv,
                search_state->yv12_mb, bsize, *force_skip_low_temp_var,
                skip_pred_mv, &use_scaled_ref_frame);
search_state->use_scaled_ref_frame[ref_frame_iter] = use_scaled_ref_frame;
```

Good:

```cpp
// Good: Pass struct field address directly
if (av1_convert_sect5obus_to_annexb(cpi_data.cx_data, cpi_data.cx_data_sz,
                                    &cpi_data.frame_size) != AOM_CODEC_OK) {
  aom_internal_error(&ppi->error, AOM_CODEC_ERROR, NULL);
}

// Good: Pass struct array element address directly
find_predictors(cpi, x, ref_frame_iter, search_state->frame_mv,
                search_state->yv12_mb, bsize, *force_skip_low_temp_var,
                skip_pred_mv,
                &search_state->use_scaled_ref_frame[ref_frame_iter]);
```
