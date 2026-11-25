# CAS work unit 044: definite integrals of powers and products of SIN and COS

**From:** casim · **To:** abacus · **Status:** DRAFT, 2026-11-25, for abacus's rulings. The number is abacus's
(#17147: CAS 042's ruling 4 left defint to the next unit, and CAS 043 took that place). Measured on a throwaway
prototype, p1, over 03441b9 (1.10.0's code), deleted once this is ruled.

- **Why now:** CAS 042 (work/042-trig-integrals.md) gave integrate these integrands; defint, which takes CAS 006's
  table only (CAS 009), still refuses every one of them. abacus's ruling 4 there: "Exact values at the bounds (SIN at
  rational multiples of π, DEG bounds) are a different risk, and they get their own hidden set."
- **The question:** can defint give the exact value of ∫ f from lo to hi for f a polynomial in SIN and COS of one angle,
  through CAS 042's checked antiderivative and CAS 009's own bounds, interval check and exact values, in the room left
  (3,314 B under 301,056)?

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
