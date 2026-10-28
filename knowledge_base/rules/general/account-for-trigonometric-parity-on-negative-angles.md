---
name: libaom/account-for-trigonometric-parity-on-negative-angles
description: When computing or looking up trigonometric values for signed rotation angles, account for parity by negating sine while preserving unnegated cosine
rubric: correctness
severity: should_fix
---

**Rule.** When computing, evaluating, or indexing lookup tables for
trigonometric values (such as sine and cosine) with signed rotation angles or
angular displacements, respect mathematical parity: negate sine values for
negative angles because sine is an odd function (sin(-x) = -sin(x)), and do not
negate cosine values because cosine is an even function (cos(-x) = cos(x)).

**Why.** Cosine is an even function (cos(-x) = cos(x)) and preserves its sign
regardless of angle polarity. Sine is an odd function (sin(-x) = -sin(x)) and
changes sign under angle negation. Negating cosine or failing to negate sine
when evaluating negative angles corrupts geometric transformation matrices,
distorts warped motion models, and inverts rotation directions.

## Examples

Bad:

```cpp
// BUG: Negating cosine for negative angles violates even parity (cos(-x) = cos(x)).
const int sine_val = sine_values[abs(rotation)];
const int cosine_val =
    (rotation < 0) ? -cosine_values[abs(rotation)] : cosine_values[abs(rotation)];
```

Good:

```cpp
// Correct: Sine is odd (sin(-x) = -sin(x)); cosine is even (cos(-x) = cos(x)).
const int sine_val =
    (rotation < 0) ? -sine_values[abs(rotation)] : sine_values[abs(rotation)];
const int cosine_val = cosine_values[abs(rotation)];
```
