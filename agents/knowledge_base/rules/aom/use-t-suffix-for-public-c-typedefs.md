---
name: libaom/aom/use-t-suffix-for-public-c-typedefs
description: When referencing or casting public C API types in headers or doc comments, use the typedef name with '_t' suffix rather than the tag name
rubric: readability
severity: nit
---

**Rule.** When referencing or casting public C API types (structs and enums
defined in aom/*.h) in headers, function signatures, or doc comments, use the
typedef name with the '_t' suffix (e.g., 'aom_metadata_insert_flags_t' or
'aom_image_t') rather than the tag name without '_t'.

**Why.** Public structs and enums in libaom headers are defined as typedefs with
a '_t' suffix (typedef enum aom_foo { ... } aom_foo_t;). In C, referencing or
casting to tag names without the 'enum' or 'struct' keyword is invalid syntax,
whereas using the '_t' typedef name ensures correct C syntax and consistent
public API documentation.

## Examples

Bad:

```cpp
/*!\brief Inserts metadata into an image.
 *
 * Example usage:
 * \code
 * insert_flag = (aom_metadata_insert_flags)(AOM_MIF_KEY_FRAME | AOM_MIF_LAYER_SPECIFIC);
 * \endcode
 */
int aom_img_add_metadata(aom_image *img, uint32_t type, const uint8_t *data,
                         size_t sz, aom_metadata_insert_flags insert_flag);
```

Good:

```cpp
/*!\brief Inserts metadata into an image.
 *
 * Example usage:
 * \code
 * insert_flag = (aom_metadata_insert_flags_t)(AOM_MIF_KEY_FRAME | AOM_MIF_LAYER_SPECIFIC);
 * \endcode
 */
int aom_img_add_metadata(aom_image_t *img, uint32_t type, const uint8_t *data,
                         size_t sz, aom_metadata_insert_flags_t insert_flag);
```
