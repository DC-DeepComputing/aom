---
name: libaom/document-enum-definitions-not-in-function-doxygen-comments
description: When documenting function parameters that accept enum types in Doxygen comments, document the enum definition directly rather than enumerating individual values under \param
rubric: readability
severity: nit
---

**Rule.** Do not enumerate or describe individual enum values inside a
function's Doxygen \param documentation comment. Instead, document the enum type
and its enumerator members directly on the enum definition using Doxygen
comments (/*!\enum, /**< ... */), allowing Doxygen to automatically generate
cross-reference links from typed parameters.

**Why.** Listing enum values in function documentation duplicates information
that belongs on the enum type definition, creating maintenance overhead and
risking documentation drift when enum members are added or modified. Documenting
the enum definition directly establishes a single source of truth and lets
Doxygen cross-link the parameter's type to its enumerator documentation.

## Examples

Bad:

```cpp
/*!\brief AV1 CDEF parameter search
 * \param[in] pick_method The method used to select params
 * \par method includes:
 * \arg \c CDEF_FULL_SEARCH: Do full search
 * \arg \c CDEF_FAST_SEARCH_LVL1: Search among a subset of all possible filters.
 */
int cdef_pick_filter(CDEF_PICK_METHOD pick_method, ...);
```

Good:

```cpp
/*!\enum CDEF_PICK_METHOD
 * \brief This enumeration defines a variety of CDEF pick methods
 */
typedef enum {
  CDEF_FULL_SEARCH,      /**< Full search */
  CDEF_FAST_SEARCH_LVL1, /**< Search among a subset of all possible filters. */
} CDEF_PICK_METHOD;

/*!\brief AV1 CDEF parameter search
 * \param[in] pick_method The method used to select params
 */
int cdef_pick_filter(CDEF_PICK_METHOD pick_method, ...);
```
