---
name: libaom/check-fread-fwrite-return-against-item-count
description: When verifying the return value of fread or fwrite, compare against the element count rather than the element byte size
rubric: correctness
severity: blocking
---

**Rule.** When checking or asserting the return value of fread or fwrite,
compare the result against the element count argument (nmemb / count), not the
element byte size (size). If verifying a total byte count, pass 1 as the element
size (size) and the total byte length as the element count (count).

**Why.** fread(ptr, size, count, stream) and fwrite(ptr, size, count, stream)
return the number of complete items successfully transferred (at most count),
not the total number of bytes. Comparing the return value against size when
count is 1 causes assertions and conditional checks to fail whenever size != 1.

## Examples

Bad:

```cpp
// Bug: fread returns the number of items (1), not the byte size (raw_size_).
ASSERT_EQ(fread(img_input_->img_data, raw_size_, 1, input_file_), raw_size_);
```

Good:

```cpp
// Check against item count (1):
ASSERT_EQ(fread(img_input_->img_data, raw_size_, 1, input_file_), 1);

// Or pass size=1 and count=raw_size_ to check byte count:
ASSERT_EQ(fread(img_input_->img_data, 1, raw_size_, input_file_), raw_size_);
```
