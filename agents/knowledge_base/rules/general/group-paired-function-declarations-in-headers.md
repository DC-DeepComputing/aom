---
name: libaom/group-paired-function-declarations-in-headers
description: When declaring counterpart or logically paired functions in headers (such as standard and _highbd variants), keep their prototypes grouped contiguously
rubric: readability
severity: nit
---

**Rule.** Keep logically paired or counterpart function declarations (such as
standard bit-depth and _highbd routines, or variant implementations) grouped
contiguously together in header files. Do not insert new or unrelated function
declarations between paired prototypes, especially when doc comments
cross-reference the sibling routine.

**Why.** Grouping paired and counterpart function declarations maintains header
organization and logical cohesion. It ensures that sibling routines and
cross-referencing doc comments (such as brief summaries directing readers to the
base function) remain adjacent and easy to understand.

## Examples

Bad:

```cpp
void av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                      int *val_count, int *num_colors);

bool av1_count_colors_with_threshold(const uint8_t *src, int stride, int rows,
                                     int cols, int num_colors_threshold,
                                     int *num_colors);

/*! \brief See \ref av1_count_colors(), but for highbd. */
void av1_count_colors_highbd(const uint8_t *src8, int stride, int rows,
                             int cols, int bit_depth, int *val_count,
                             int *val_count_8bit, int *num_color_bins,
                             int *num_colors);
```

Good:

```cpp
void av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                      int *val_count, int *num_colors);

/*! \brief See \ref av1_count_colors(), but for highbd. */
void av1_count_colors_highbd(const uint8_t *src8, int stride, int rows,
                             int cols, int bit_depth, int *val_count,
                             int *val_count_8bit, int *num_color_bins,
                             int *num_colors);

bool av1_count_colors_with_threshold(const uint8_t *src, int stride, int rows,
                                     int cols, int num_colors_threshold,
                                     int *num_colors);
```
