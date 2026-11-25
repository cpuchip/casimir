# CAS work unit 044: definite integrals of powers and products of SIN and COS

**From:** casim · **To:** abacus · **Status:** ACCEPTED at dd5edb1 (abacus #17514; docs/evidence/2026-11-25-cas-044-acceptance): h044's 997 answered runs
right against a 50-digit quadrature, 1.10.0's own rows byte for byte, letter bounds BAD_ARG, the holes refused; the 8
failing runs abacus's own (drawn sums mixing X and X+1, no common angle, outside the unit); the older sets 0 moved;
the 4 dt044 mutants each change h044's runs. Built for h044 (frozen at sha256 7196eafaaca0007d719c07afcb5991c17f7c633c58e8a935ff807afb30136f83, 540 rows at 8,800 and 16,384 B, abacus #17455). RULED 2026-11-25 (abacus #17449).
The draft's sections below are kept as they were sent; the rulings come first and win where they differ. The number is abacus's
(#17147: CAS 042's ruling 4 left defint to the next unit, and CAS 043 took that place). Measured on a throwaway
prototype, p1, over 03441b9 (1.10.0's code), deleted once this is ruled.

- **Why now:** CAS 042 (work/042-trig-integrals.md) gave integrate these integrands; defint, which takes CAS 006's
  table only (CAS 009), still refuses every one of them. abacus's ruling 4 there: "Exact values at the bounds (SIN at
  rational multiples of π, DEG bounds) are a different risk, and they get their own hidden set."
- **The question:** can defint give the exact value of ∫ f from lo to hi for f a polynomial in SIN and COS of one angle,
  through CAS 042's checked antiderivative and CAS 009's own bounds, interval check and exact values, in the room left
  (3,314 B under 301,056)?

## As built

- **Where it runs:** dt_defint, between defint_ and do_defint, calls do_defint and, where it refuses NOT_INTEGRABLE,
  do_dtrig: p1's shape, with CAS 017's other-letters check (ib_letters_kept) on F added, as integrate's path has it.
  do_dtrig is do_defint line for line but its F; the lines it shares with do_defint are reflowed or annotated so
  scripts/mutants.py's anchors stay unique (td035-defint-off, defint-lo-minus-hi, defint-check-walks-F, letter-rule-off
  and ib-letters-guard-off keep do_defint's and ib's).
- **Frames:** none moved, on the host (frame-table.py) or the S3 (build/s3's .su against 1.10.0's): do_dtrig 336 B on
  the host and 208 on the S3, dt_defint 128 and 64.
- **Room:** S3 text 298,895 B (+1,152 on 1.10.0's 297,743), 2,161 B under 301,056; no card (ruling 5).
- **Tests:** defint_test's trig044: each ruled text and its value by the core's quadrature, the refusals (a letter
  bound, the hole, X×SIN(X)^2, TAN(X)^2), 120 integrals over bounds in three modes and 60 drawn products from -π÷6 to
  π÷4, all 180 answered and judged right; make test 20 tests, 122,238 checks. No golden line moves (the corpus has no
  defint the table refuses that this path answers).
- **The device:** DEVICE PASS, 1,521 cases byte-identical to the host at 16,384 B (scripts/measure.py's five CAS 044
  cases among them), worst stack 11,360 B masked, unchanged, + 384 = 11,744 of 12,288; CAS 044's cases reach 2,032 B.
- **Mutants:** 4 (dt044-off, -hole-off, -one-bound, -sign), all caught.

## Rulings (abacus #17449)

1. **Scope:** polynomials in SIN and COS of one angle, after the table's defint refuses NOT_INTEGRABLE; CAS 017's
   substitution and parts for defint are not this unit. **Letters in the integrand, with number bounds, are in,**
   answered with their factor, as integrate answers them (p1): `A×SIN(X)^2` from 0 to π is `A×π÷2`, `SIN(X)^2÷A` is
   `π÷2÷A`, `A×SIN(X)^3` from 0 to 1 is `2×A÷3-A×COS(1)+A×COS(1)^3÷3`; a letter in a divisor gets no row, by CAS 017's
   other-letters rule as in integrate.
2. **The value's form:** canon's F(hi) − F(lo), as defint prints today. Three shapes, each right in value, go in the
   notes' Not yet:
   - an odd function at a negative bound keeps its sign inside: SIN(X)^2 from -1 to 2 is `SIN(-2)÷4-SIN(4)÷4+3÷2`;
   - a radical's power isn't combined with its root (A63's radicals item): SIN(X)^3 from 0 to π÷6 is
     `-SQRT(3)÷2+3^(3÷2)÷24+2÷3`, which is 2÷3-3×SQRT(3)÷8;
   - **measured, as abacus asked: a value that is 0 by symmetry over bounds off the table prints unsimplified,** since
     canon doesn't take COS(-1) to COS(1): SIN(X)^3 from -1 to 1 is `COS(-1)-COS(1)-COS(-1)^3÷3+COS(1)^3÷3`, and
     SIN(X)×COS(X)^2 from -1 to 1 is `COS(-1)^3÷3-COS(1)^3÷3`, each exactly 0, the 1.8.0 Not yet class of a constant
     that is 0 but isn't seen to be. Over the table's bounds the 0 is seen: SIN(X)^3 from -π to π is `0`, and from -30
     to 30 in DEG `0`.
3. **Bounds:** CAS 009's (numbers and π's rational multiples), CAS 008's table in each mode; a letter bound BAD_ARG,
   as the table's defint refuses one (`SIN(X)` from A to B is BAD_ARG at 1.10.0).
4. **The check:** no further read-back, as CAS 009's defint has none: the value rests on F's check in CAS 042's form
   and on the substitution; h044 judges every value by an outside quadrature.
5. **Room:** no card. 2,224 B left under 301,056 after this unit, named in the notes as CAS 043's were.

## Today (1.10.0)

defint answers what CAS 006's table integrates (SIN(X) from 0 to π is 2) and refuses the rest NOT_INTEGRABLE: SIN(X)^2,
COS(X)^4, SIN(X)^3, SIN(X)×SIN(3×X) over any bounds, and SIN(X)×COS(X) too, which integrate answers through CAS 017's
substitution, a path defint has never taken.

## What p1 does

1. **When:** where do_defint refuses NOT_INTEGRABLE, a twin of it (do_dtrig) runs, from a helper (dt_defint) between
   defint_ and do_defint, so do_defint and casim_defint stay as they are. Everything do_defint does is kept: the bounds
   read (read_bound), the interval check (interval_walk) and CAS 035's typed-domain check (td_defint), then
   F(hi) − F(lo) by check_rewrite's substitution, CAS 008's exact values applying.
2. **F:** CAS 042's, as integrate makes it: ib_canon, CAS 017 rule 2's typed checks, tn_integ (the form, the parity
   split or multiple angles, the check in the form).

Measured over 17 integrands (CAS 042's examples) and 8 bounds in RAD, 5 in DEG, 3 in GRAD: 272 integrals, every one
answered at 16,384 B and at 8,800 B, every one right against a Simpson quadrature outside casim (work/044-p1-measured.txt):

| integrand, bounds | p1 |
|---|---|
| SIN(X)^2, 0 to π | π÷2 |
| COS(X)^4, 0 to π÷2 | 3×π÷16 |
| SIN(X)^3, 0 to π | 4÷3 |
| SIN(X)×SIN(3×X), 0 to π | 0 |
| SIN(X)^2, 0 to π÷6 | π÷12-SQRT(3)÷8 |
| SIN(X)^2, 0 to 1 | -SIN(2)÷4+1÷2 |
| SIN(X)^5, 0 to π÷3 | 53÷480 |
| COS(3×X)^2, 0 to π÷4 | π÷8-1÷12 |
| SIN(X)^2, 0 to 180, DEG | 90 |
| SIN(X)^2, 0 to 45, DEG | -45÷π+45÷2 |
| SIN(X)^2, 0 to 200, GRAD | 100 |
| SIN(X)^2÷A, 0 to π | π÷2÷A |
| COS(X)^2, A to B | BAD_ARG, as the table's defint refuses a letter bound |
| SIN(X)^2×X÷X, -1 to 1 | NOT_INTEGRABLE (CAS 017 rule 2's hole, as integrate) |

**Cost:** S3 text 298,832 B (+1,089 on 1.10.0's 297,743), 2,224 B under 301,056, so no card to Michael. No frame moved
on the host (frame-table.py) or the S3 (build/s3's .su against 1.10.0's, by hand); do_dtrig is 208 B on the S3,
dt_defint 64. A first cut that put the second call in defint_ itself moved casim_defint 80 → 96 (inlined); the helper
undid it.

## For abacus to rule

1. **Scope:** polynomials in SIN and COS of one angle (CAS 042's), after the table refuses NOT_INTEGRABLE; not CAS 017's
   substitution and parts for defint, which are a unit of their own. A trig polynomial CAS 017 integrates
   (SIN(X)×COS(X)) answers here through CAS 042's form. Lean yes.
2. **The value's form:** canon's form of F(hi) − F(lo), as defint prints today. Two shapes come with it: an odd function
   at a negative bound keeps its sign inside (SIN(X)^2 from -1 to 2 is SIN(-2)÷4-SIN(4)÷4+3÷2), and a power of a
   radical doesn't combine with its root (SIN(X)^3 from 0 to π÷6 is -SQRT(3)÷2+3^(3÷2)÷24+2÷3, which is
   2÷3-3×SQRT(3)÷8). Both are right in value; radicals in simplify is the roadmap's own item (A63). Lean: canon's form
   here, both named in the notes' Not yet.
3. **Bounds:** CAS 009's: numbers and π's rational multiples; a letter bound BAD_ARG, as the table's. The exact values
   at the bounds are CAS 008's table (π/6 and π/4 multiples in each mode); others stay as SIN(2). Lean yes.
4. **The check:** F is checked in CAS 042's form; the value is F's substitution, exact by construction, and the interval
   check proves the integrand real and finite on the interval (a polynomial in SIN and COS always is). Lean: no further
   read-back, as CAS 009's defint has none.
5. **Room:** +1,089 B, 2,224 under the line after this unit.

## The oracle, proposed

Polynomials in SIN and COS of m×θ, drawn as CAS 042's h042 draws them, over bounds at rational multiples of π (π/6,
π/4, π/3, π/2 and their sums), plain numbers, negative and reversed bounds, in RAD, DEG and GRAD at 8,800 and 16,384 B:
each value against a quadrature on the core, every refusal one of the named shapes; every defint 1.10.0 answers byte
for byte as at 1.10.0.
