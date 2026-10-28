---
name: libaom/use-established-terminology-in-doxygen-param-comments
description: When writing Doxygen parameter doc comments, use established codebase terminology and maintain consistent phrasing and grammatical style across adjacent parameters
rubric: readability
severity: nit
---

**Rule.** When writing Doxygen parameter descriptions and doc comments (\param
or @param), use established codebase terminology (such as 'visual tuning metric'
for tuning) rather than novel or informal phrasings, and maintain consistent
grammatical style across adjacent parameters (e.g. prefixing boolean parameter
descriptions with 'Flag ...' when neighboring entries do so). Consult
surrounding doc blocks and existing header definitions to keep parameter
documentation uniform across the codebase.

**Why.** Inconsistent phrasing or idiosyncratic terminology in header docstrings
degrades documentation readability, confuses API consumers, and impairs
searchability. Standardizing terminology and maintaining uniform grammatical
structure keeps public and internal interfaces clean and easy to scan.

## Examples

Bad:

```cpp
// Variant 1: Inconsistent phrasing across adjacent boolean flags
/*!\brief Pick CDEF filtering parameters from QP.
 * \param[in]      skip_cdef          Flag to skip CDEF filtering
 * \param[in]      is_screen_content  Flag indicating screen content
 * \param[in]      avoid_uv_cdef      Avoid assigning UV CDEF strengths
 */
void av1_pick_cdef_from_qp(AV1_COMMON *const cm, int skip_cdef,
                           int is_screen_content, bool avoid_uv_cdef);

// Variant 2: Novel or non-standard terminology
/*!\brief Compute rdmult based on q index and frame update type.
 * \param[in]       tuning          metric tuning
 */
int av1_compute_rd_mult_based_on_qindex(...);
```

Good:

```cpp
// Variant 1: Consistent phrasing across adjacent boolean flags
/*!\brief Pick CDEF filtering parameters from QP.
 * \param[in]      skip_cdef          Flag to skip CDEF filtering
 * \param[in]      is_screen_content  Flag indicating screen content
 * \param[in]      avoid_uv_cdef      Flag to avoid assigning UV CDEF strengths
 */
void av1_pick_cdef_from_qp(AV1_COMMON *const cm, int skip_cdef,
                           int is_screen_content, bool avoid_uv_cdef);

// Variant 2: Established codebase terminology
/*!\brief Compute rdmult based on q index and frame update type.
 * \param[in]       tuning          visual tuning metric
 */
int av1_compute_rd_mult_based_on_qindex(...);
```
