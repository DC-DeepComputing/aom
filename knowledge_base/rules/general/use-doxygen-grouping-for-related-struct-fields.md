---
name: libaom/use-doxygen-grouping-for-related-struct-fields
description: When documenting a group of related struct or class fields in headers, use Doxygen member grouping (\name, /**@{*/, /**@}*/) instead of plain comments
rubric: readability
severity: nit
---

**Rule.** When documenting a group or section of related struct or class fields
in header files, enclose them in Doxygen member grouping blocks using `/** \name
<Description> */` (or `/*! \name ... */`) followed by `/**@{*/` and closing with
`/**@}*/`. Do not use plain non-Doxygen line comments (`//`) to introduce
grouped fields.

**Why.** Plain line comments are ignored by Doxygen and fail to associate the
group description with enclosed struct members in generated documentation.
Member grouping syntax ensures the section heading and description are properly
attached to all enclosed fields in API and internal reference docs.

## Examples

Bad:

```cpp
// Fields below here are allocated and initialised by
// av1_alloc_restoration_struct. (horz_)units_per_tile give the number of
// restoration units in (one row of) the largest tile in the frame.
int units_per_tile;
int vert_units_per_tile;
int horz_units_per_tile;
```

Good:

```cpp
/**
 * \name Fields allocated and initialised by av1_alloc_restoration_struct.
 * (horz_)units_per_tile give the number of restoration units in
 * (one row of) the largest tile in the frame.
 */
/**@{*/
int units_per_tile;
int vert_units_per_tile;
int horz_units_per_tile;
/**@}*/
```
