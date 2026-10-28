---
name: libaom/serialize-multibyte-integers-before-hashing
description: When computing hash or CRC checksums over multi-byte integer arrays, serialize elements into canonical byte order instead of casting raw memory
rubric: correctness
severity: nit
---

**Rule.** Do not compute hash or CRC checksums directly over multi-byte integer
arrays (such as uint16_t or uint32_t) or structs cast to uint8_t * without
serializing them in a canonical byte order. If an endian-dependent in-memory
representation is temporarily used, document the architecture dependency with an
explanatory comment and tracking TODO.

**Why.** Reinterpreting multi-byte integers as raw byte streams makes the
resulting hash digest or checksum dependent on host CPU endianness
(little-endian vs. big-endian). This causes non-deterministic behavior and
divergent encoder/decoder decisions across platforms.

## Examples

Bad:

```cpp
uint32_t to_hash[4];
// ... populate to_hash ...
// Endian-dependent: produces different CRC digests on big-endian CPUs.
uint32_t hash =
    av1_get_crc32c_value(calc, (uint8_t *)to_hash, sizeof(to_hash));
```

Good:

```cpp
// Option 1: Canonical byte serialization before hashing
uint8_t byte_buf[16];
for (int i = 0; i < 4; ++i) {
  byte_buf[i * 4 + 0] = (uint8_t)(to_hash[i] >> 0);
  byte_buf[i * 4 + 1] = (uint8_t)(to_hash[i] >> 8);
  byte_buf[i * 4 + 2] = (uint8_t)(to_hash[i] >> 16);
  byte_buf[i * 4 + 3] = (uint8_t)(to_hash[i] >> 24);
}
uint32_t hash = av1_get_crc32c_value(calc, byte_buf, sizeof(byte_buf));

// Option 2: Document temporary endianness dependency with tracking TODO
// TODO(aomedia:...): Serialize input values to be endian-independent.
uint32_t hash =
    av1_get_crc32c_value(calc, (uint8_t *)to_hash, sizeof(to_hash));
```
