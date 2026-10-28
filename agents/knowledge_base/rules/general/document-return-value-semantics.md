---
name: libaom/document-return-value-semantics
description: Document function declarations with purpose, return semantics, Doxygen \retval categories, out-parameter contracts, and context error side effects (cpi->common.error)
rubric: readability
severity: should_fix
---

**Rule.** When adding or declaring functions in header files, place interface
doc comments on the header declaration rather than the source definition
(reserving source comments for non-obvious implementation details). Precede
declarations with comments describing their purpose, return semantics, parameter
contracts, and side effects:
- **Return semantics & Doxygen tags**: Explicitly document what each return value or status code indicates (such as boolean 1/0, negative error codes, or `aom_codec_err_t` / `AOM_CODEC_OK`) so callers do not have to guess between boolean and error-status conventions, especially for plain `int` return types. In Doxygen comments, document return value conditions and status codes using `\retval` entries rather than embedding them inside `\remark` or `\return` paragraphs. Wrap multi-word or range descriptions in quotes (e.g., `\retval "A nonzero (positive) aom_codec_err_t code"`), and use `\c` formatting for inline identifier references.
- **Out-parameter contracts**: Specify conditional postconditions and status out-parameters (e.g., `*error`, `*status`, `*valid`, `*found`, `*size`), clarifying whether they are always updated or only valid on success/failure.
- **Error side effects**: When functions populate diagnostic details in caller context error structures (such as `cpi->common.error`, `pbi->common.error`, or `struct aom_internal_error_info *`) via `aom_internal_error()` or macros upon failure, explicitly document these error-reporting side effects alongside return value contracts in doc comments and Doxygen `\retval` / `\return` tags.
- **Parameter tuning**: Document non-obvious or mode-dependent parameters (such as REALTIME vs. non-realtime behavior and exceptions).

**Why.** Placing interface doc comments on header declarations allows callers to
understand function contracts without inspecting implementation files. In C
APIs, `int` return types are ambiguous between boolean (1 for true) and
POSIX/status conventions (0 for success, negative or `AOM_CODEC_ERROR` on
failure). Doxygen compiles `\retval` tags into a dedicated, structured return
value table, whereas embedding error codes in `\remark` hides them from the
function's return specification. Clearly documenting return codes, out-parameter
validity, context error population (`cpi->common.error`), and
configuration-dependent argument tuning prevents callers from misinterpreting
return values, reading uninitialized out-parameters, or missing error-handling
contracts.

## Examples

Bad:

```cpp
// 1. Ambiguous int return semantics and undocumented header declaration:
int av1_convert_q_to_qindex(double q, aom_bit_depth_t bit_depth);
int aom_calc_butteraugli(const YV12_BUFFER_CONFIG *src, const YV12_BUFFER_CONFIG *dist, ...);

// 2. Return categories and error side effects hidden in \remark instead of \retval:
/*!\brief Encodes a frame.
 * \return Returns a value to indicate if the encoding is done successfully.
 * \retval #AOM_CODEC_OK
 * \retval -1 No frame encoded; more input is required.
 * \remark Sets the error code and error message in \c cpi->common.error and
 * returns error code on failure. */
aom_codec_err_t av1_encode_frame(AV1_COMP *cpi);

// 3. Undocumented out-parameter contract and context error side effects:
bool parse_line(FILE *file, Value *val, bool *syntax_error);
bool av1_tf_info_alloc(TEMPORAL_FILTER_INFO *tf_info, const struct AV1_COMP *cpi);
```

Good:

```cpp
// 1. Explicit return semantics and purpose in header declaration:
// Converts a quantizer (q) to a qindex.
int av1_convert_q_to_qindex(double q, aom_bit_depth_t bit_depth);
// Returns 1 (true) on success or 0 (false) on failure.
int aom_calc_butteraugli(const YV12_BUFFER_CONFIG *src, const YV12_BUFFER_CONFIG *dist, ...);

// 2. Structured \retval tags specifying return codes and error side effects:
/*!\brief Encodes a frame.
 * \return Returns a value to indicate if the encoding is done successfully.
 * \retval #AOM_CODEC_OK Success.
 * \retval -1 No frame encoded; more input is required.
 * \retval "A nonzero (positive) aom_codec_err_t code"
 *     The encoding failed with the error. Sets the error code and error message
 *     in \c cpi->common.error. */
aom_codec_err_t av1_encode_frame(AV1_COMP *cpi);

// 3. Explicit out-parameter postconditions and context error contracts:
// Parses next line into 'val'. Returns true on success. Sets 'syntax_error' to true and returns false on error.
bool parse_line(FILE *file, Value *val, bool *syntax_error);
// Returns true on success. Sets error code and message in cpi->common.error and returns false on failure.
bool av1_tf_info_alloc(TEMPORAL_FILTER_INFO *tf_info, const struct AV1_COMP *cpi);
```
