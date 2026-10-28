---
name: libaom/normalize-angles-modulo-two-pi
description: When wrapping or normalizing angular coordinates into symmetric ranges like [-PI, PI], compute modulo 2*PI rather than modulo PI
rubric: correctness
severity: should_fix
---

**Rule.** When wrapping or normalizing periodic angular coordinates into a
symmetric range (such as [-PI, PI] or [-180, 180] degrees), compute the
remainder modulo 2*PI (e.g., using fmod(angle, 2 * PI)) and adjust out-of-range
values by adding or subtracting 2*PI. Do not take modulo PI (or 180), which
erroneously halves the fundamental angular period and conflates opposite
quadrants.

**Why.** Planar, spherical, and rotational angles have a fundamental period of
2*PI (360 degrees). Taking modulo PI maps distinct angles separated by odd
multiples of PI (such as -1.5*PI and -0.5*PI, or opposite quadrants) to
identical values, corrupting coordinate transformations and geometric
projections.

## Examples

Bad:

```cpp
// BUG: fmod(theta, PI) halves the 2*PI period, mapping -1.5*PI to -0.5*PI.
double theta_mod = fmod(theta, PI);
if (theta_mod == 0 && fmod(theta, 2 * PI) != 0) {
  if (theta > 0) {
    theta_mod = PI;
  } else if (theta < 0) {
    theta_mod = -PI;
  }
}
```

Good:

```cpp
// Correct: Wrap modulo 2*PI and shift into [-PI, PI].
double theta_mod = fmod(theta, 2 * PI);
if (theta_mod < -PI) {
  theta_mod = theta_mod + 2 * PI;
} else if (theta_mod > PI) {
  theta_mod = theta_mod - 2 * PI;
} else if (theta_mod == PI) {
  theta_mod = -PI;
}
```
