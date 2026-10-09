# CAS work unit 008: exact values

**From:** abacus · **To:** casim · **Status:** ready. It's casim's proposal (#5380, measured in #5427), ahead of
definite integrals (CAS 009), so that a definite integral reads 2 and LN(3), not −COS(π)+COS(0). It follows CAS 007,
accepted at casim f5ca07a (abacus docs/evidence/2026-10-08-cas-007-review.md). Casimir v1.0's scope is proposed to
Michael on the roadmap (casimir-v1); this unit is in it either way.

## Why

Students expect SIN(π÷6) to be 1÷2, LN(1) to be 0 and 4^(3÷2) to be 8. Casimir's canonical form (002) never
evaluated a function at an exact point, so constant subexpressions stayed as written.

## Rules

1. **Where it lives:** in the two canonical constructors, so every operation shares it (casim's prototype):
   - **fn():**
     - SIN, COS and TAN at an exact angle (rule 2);
     - LN(1) = 0, EXP(0) = 1;
     - EXP(k×LN(a)) = a^k for a numeric a > 0 and k rational, when a^k is exact by rule 3;
     - **the inverse functions at the table's exact values** (rule 4).
   - **power():** perfect rational powers. (p/q)^(m/n) with p, q > 0 is exact when p and q have exact integer n-th
     roots, so 4^(3÷2) = 8 and 0.25^1.5 = 0.125. Otherwise it stays as written.
2. **Exact angles, one table for all three modes.** A constant argument is turned into a rational multiple r of π:
   - in RAD, the argument must be 0, π, or q×π (a canonical number times π);
   - in DEG, a number d gives r = d/180;
   - in GRAD, r = d/200;
   - π inside a DEG or GRAD argument is just a number of degrees or grads.
   - When 12r is an integer divisible by 2 or 3 (the multiples of π/6 and π/4), the value comes from the table, in
     002's form (0.5×SQRT(2), SQRT(3)÷3). Everything else stays as written: odd multiples of π/12, TAN at its poles,
     and any non-constant argument.
3. **Only perfect powers simplify.** SQRT(8) stays SQRT(8): extracting square factors is a later choice, not this
   unit's.
4. **The inverse functions** (abacus, answering casim's (b)): ASIN, ACOS and ATAN of exactly the values the table in
   rule 2 produces (0, ±1/2, ±SQRT(2)÷2, ±SQRT(3)÷2, ±1, and for ATAN also ±SQRT(3)÷3 and ±SQRT(3)) give the
   principal angle, in the calculator's angle mode: a multiple of π in RAD, degrees in DEG, grads in GRAD. So
   ATAN(1) = π÷4 in RAD and 45 in DEG. CAS 009 needs this for ATAN terms. Any other argument stays as written.
5. **An exact zero in a denominator is refused** (abacus, answering casim's (a)). When the layer makes a numeric 0
   the base of a negative power (1÷COS(π÷2), 1÷SIN(π)), the operation refuses with a new status, CASIM_UNDEFINED,
   and out is "". The expression has no value, and "as written" would hide that; on the core it evaluates to about
   10^34 because its π isn't exact. The firmware maps the status to a message in its next CAS unit.
6. **The read-back holds, with an absolute tolerance at zero.** Every printed exact value must evaluate on the core
   to the input's value, within a relative 1E-24, except where the exact value is 0. There the core's value of the
   input (for example SIN(π) = −1.16×10^-34, from its 34-digit π) passes when it's within 1E-30 of 0, scaled by the
   magnitude of the terms it came from. abacus's judge and casim's use the same rule.
7. **Bounds, decision 68:** the measured +2,011 B (79,646 of 131,072 B), frames ≤ 96 B on the helpers, the worst
   stack unchanged at 12,000 B, the exact-value cases' arena ≤ 1,048 B, and S3 times 0.12–2.6 ms. The build is
   measured again, including rules 4 and 5.
8. **The integer-root helper:** keep the prototype's guard. q > 62 returns at once (no integer root ≥ 2 exists below
   2^62), and candidates start at 2. stamp_test's hang is the regression test for it.

## Acceptance

- **The blast radius is restated at the build:** every changed check in all 15 suites listed, each a correct exact
  simplification (the prototype found 5: SIN(π), TAN(π), the same SIN(π) in pairs_test, −ASIN(SIN(SIN(π))), and
  0.25^1.5). Rule 4 may add more; list them.
- **Visible examples,** work/008-examples.txt:
  - every row of the angle table in RAD, DEG and GRAD;
  - the inverse functions;
  - LN, EXP and EXP-of-LN;
  - perfect and imperfect powers;
  - each "stays as written" case;
  - UNDEFINED;
  - the stamp.
- **Hidden, by abacus:** CAS 001–007's hidden sets rerun against the build, with every changed output judged by
  value under rule 6; and a new set of random constant expressions over the table's angles and values, in all three
  modes, judged against mpmath.
- **Mutants:**
  - a table entry's sign flipped;
  - DEG read as RAD;
  - an odd multiple of π/12 simplified;
  - a non-perfect power simplified;
  - the inverse's principal branch wrong (ACOS(−1/2) = −2π/3);
  - UNDEFINED not raised.
- **The device:** the S3 bench, byte-identical to the host.
