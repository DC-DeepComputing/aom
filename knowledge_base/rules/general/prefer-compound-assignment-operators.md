---
name: libaom/prefer-compound-assignment-operators
description: When updating a variable in place, use compound assignment operators like += and <<= instead of self-assignment, and prefer ++ or -- for unit modifications
rubric: readability
severity: nit
---

**Rule.** Prefer compound assignment operators (such as +=, -=, *=, /=, <<=,
>>=, &=, |=, ^=) over explicit self-assignment (such as x = x + y or x = x << y)
when modifying a variable in place. Additionally, for single-unit modifications,
prefer unary increment (++) and decrement (--) operators rather than compound
assignment (+= 1, -= 1) or explicit unit arithmetic (x = x + 1, x = x - 1).

**Why.** Compound assignment operators avoid repeating variable names or complex
lvalue expressions, making code more concise, easier to read, and less prone to
copy-paste typos. For unitary changes, unary increment and decrement operators
are the standard idiomatic expressions in C and C++, communicating single-step
modifications more clearly.

## Examples

Bad:

```cpp
// Avoid repeating the variable name in self-assignment
thresh_sad = thresh_sad << 4;
total_count = total_count + step;

// Avoid compound or self-assignment for unit modifications
height -= 1;
index += 1;
count = count + 1;
```

Good:

```cpp
// Use compound assignment operators for in-place modifications
thresh_sad <<= 4;
total_count += step;

// Use unary increment and decrement operators for unit changes
height--;
index++;
count++;
```
