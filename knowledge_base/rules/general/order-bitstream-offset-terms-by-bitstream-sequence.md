---
name: libaom/order-bitstream-offset-terms-by-bitstream-sequence
description: When computing buffer offsets, remaining byte counts, or cumulative consumed sizes across bitstream fields, arrange arithmetic operands in wire sequence order
rubric: readability
severity: nit
---

**Rule.** When calculating pointer offsets (e.g., `data + len1 + len2`),
remaining byte counts (`bytes_available - len1 - len2`), or cumulative consumed
sizes (`*bytes_read = len1 + len2 + ...`) across bitstream syntax fields or
serialized structures, list the operand terms in the exact sequential order in
which the fields appear in the bitstream wire layout (e.g., prefix length,
header size, then payload length). Do not reorder terms by category or variable
name when that differs from the wire format.

**Why.** Arranging operand terms and summands in exact wire-format sequence
mirrors the codec specification, making buffer arithmetic and size calculations
straightforward to follow and verify against bitstream syntax tables during
review.

## Examples

Bad:

```cpp
// Operands and summands listed out of bitstream wire sequence order
status = read_obu_size(
    data + obu_header->size + length_field_size_obu,
    bytes_available - obu_header->size - length_field_size_obu,
    payload_size, &length_field_size_payload);

*bytes_read = obu_header->size + length_field_size_payload +
              length_field_size_obu;
uint8_t *payload_ptr = data + payload_size + header_size;
```

Good:

```cpp
// Operands and summands follow exact wire sequence: prefix -> header -> payload
status = read_obu_size(
    data + length_field_size_obu + obu_header->size,
    bytes_available - length_field_size_obu - obu_header->size,
    payload_size, &length_field_size_payload);

*bytes_read = length_field_size_obu + obu_header->size +
              length_field_size_payload;
uint8_t *payload_ptr = data + header_size + payload_size;
```
