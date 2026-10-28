---
name: libaom/increment-retry-counter-in-loop-body-not-condition
description: When writing bounded retry or polling while loops, compare counters in the condition and increment inside the body instead of using inline ++counter in the condition
rubric: correctness
severity: should_fix
---

**Rule.** When writing retry, polling, or bounded while loops, compare the
counter against its upper limit in the loop condition and increment the counter
inside the loop body. Avoid mutating counter variables directly within compound
loop conditions (such as while (... && ++try_count <= kMaxTries)).

**Why.** Incrementing a counter within a compound while loop condition
increments the variable during the final condition evaluation that terminates
the loop without executing the body. Any post-loop error logging, diagnostic
reporting, or assertions inspecting the counter will report an off-by-one count
of attempted operations.

## Examples

Bad:

```cpp
int try_count = 0;
while (!feof(file) && len < size && ++try_count <= kMaxTries) {
  const size_t n = fread((uint8_t *)buf + len, 1, size - len, file);
  len += n;
}
if (len < size) {
  fprintf(stderr, "Failed after %d attempts\n", try_count);  // Off-by-one count
}
```

Good:

```cpp
int try_count = 0;
while (!feof(file) && len < size && try_count < kMaxTries) {
  ++try_count;
  const size_t n = fread((uint8_t *)buf + len, 1, size - len, file);
  len += n;
}
if (len < size) {
  fprintf(stderr, "Failed after %d attempts\n", try_count);  // Accurate count
}
```
