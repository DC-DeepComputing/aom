---
name: libaom/update-doxygen-parameter-documentation
description: In Doxygen doc comments, separate \brief with a blank line, match signature parameter names, use strict \param[in,out]/\param[out] syntax, and use capitalized noun phrases
rubric: readability
severity: should_fix
---

**Rule.** In Doxygen doc comments (`/*!\brief ... */` or `/**\brief ... */`):

1. **Brief Summary and Formatting**: Keep the `\brief` summary to a single concise sentence and insert a blank comment line (` *`) before detailed descriptions, lists, commands, or tags. In `\brief` summaries, distinguish out-parameters from return values (e.g., 'Set `*param` to ...' rather than 'Return ...'), and describe boolean flags by logical state ('is set' rather than literal '1').
2. **Signature Synchronization**: Synchronize doc annotations with the function signature: refer to parameters in `\brief` and `\param` tags by their exact declared names (avoid shorthand or outdated names), add entries for new parameters, and remove obsolete ones.
3. **Directionality & Tag Syntax**: Annotate pointer, buffer, or struct parameters modified or written by the function with `\param[out]` or `\param[in,out]` instead of `\param[in]`. Bidirectional tags must strictly use `\param[in,out]` (comma-separated with no spaces or slashes; never `\param[in, out]` or `\param[in/out]`). Mutable error contexts (such as `struct aom_internal_error_info *error_info`) must be documented as `\param[in,out]` with descriptions specifying their role in reporting errors (e.g., 'Error info for reporting errors' rather than 'tracking errors').
4. **Phrasing & Capitalization**: Capitalize the first letter of each description and phrase `\param` and `\return` descriptions concisely as noun phrases (e.g., 'Limits of the unit', 'Output restored buffer', 'Status code') rather than lowercase fragments or sentences beginning with filler verbs like 'provides', 'contains', 'gives', 'holds', 'specifies', or 'determines'. Document return values (`\return`) along with any early-exit caveats or contract guarantees.

**Why.** Doxygen includes all comment text up to the first blank line in the
`\brief` summary for API indices and summary tables; omitting the blank line
causes detailed paragraphs to clutter summary listings. Synchronizing parameter
names between comments and function signatures eliminates ambiguity for callers
and avoids documentation generator warnings. Using accurate direction tags
(`\param[out]`, `\param[in,out]`) and strictly formatted syntax without
whitespace or slashes ensures consistent parsing and prevents misleading callers
about in-place buffer mutations or error context side effects. Capitalized noun
phrases without redundant filler verbs keep API documentation concise,
scannable, and uniform across the codebase.

## Examples

Bad:

```cpp
// Example 1: Missing blank line after \brief, mismatched param names, and missing \return contract
/*!\brief Given spherical motion vector (phi, theta), find matching pixels.
   Exits early if the search radius exceeds threshold.
 * \param[in]   delta_phi   latitude motion vector
 * \param[in]   delta_theta longitude motion vector
 */
int av1_get_pred_erp(double delta_phi, double delta_theta);

// Example 2: Non-standard tag syntax, filler verbs, lowercase phrasing, and improper directionality
/*!\brief Filter and restore a single frame restoration unit.
 *
 * \param[in]      cm           pointer to top level common structure
 * \param[in]      limits       limits of the unit
 * \param[in/out]  frame        contains the input and output frame buffer
 * \param[in, out] error_info   error info for tracking errors
 * \return                      status code
 */
int av1_restore_unit(AV1_COMMON *cm,
                     const RestorationTileLimits *limits,
                     YV12_BUFFER_CONFIG *frame,
                     struct aom_internal_error_info *error_info);
```

Good:

```cpp
// Example 1: Blank line after \brief, exact signature names, capitalized noun phrases, and \return
/*!\brief Given spherical motion vector (delta_phi, delta_theta), find matching pixels.
 *
 * Exits early if the search radius exceeds threshold.
 * \param[in]   delta_phi   Latitude motion vector
 * \param[in]   delta_theta Longitude motion vector
 * \return                  Status code, or -1 if search exceeds threshold
 */
int av1_get_pred_erp(double delta_phi, double delta_theta);

// Example 2: Strict \param[in,out] syntax, capitalized noun phrases, and error reporting description
/*!\brief Filter and restore a single frame restoration unit.
 *
 * \param[in,out]  cm           Pointer to top-level common structure
 * \param[in]      limits       Limits of the unit
 * \param[in,out]  frame        Input and output frame buffer
 * \param[in,out]  error_info   Error info for reporting errors
 * \return                      Status code
 */
int av1_restore_unit(AV1_COMMON *cm,
                     const RestorationTileLimits *limits,
                     YV12_BUFFER_CONFIG *frame,
                     struct aom_internal_error_info *error_info);
```
