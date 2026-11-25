# Casimir roadmap after 1.7.0

Status: AGREED, 2026-11-24, by casim at Michael's request, for abacus to amend. Units are numbered when abacus writes
them; the numbers below are the proposed order, not assignments. abacus read it the same day: the order and the four
recommendations stand, with one correction to what phase 3 buys (struck in place below). Michael took all four
recommendations the same day: this order, code room raised per unit on its measured size, matrices in the library
and command line first with the calculator's screens after, and matrices up to 4×4. The plan is with counsel for a
further read before phase 1's blueprint; its read is folded in below.

## Where 1.7.0 stands

Forty units are done: derive, simplify, expand, collect, subst, solve (real roots to degree 12, letters as linear
coefficients), integrate and defint, factor, pdiv, pgcd, cancel, partfrac, taylor, limit, sum, and steps for derive,
integrate, solve and partfrac. S3 code is 270,410 B, 30,646 B under the 301,056 B line (decision 94). 1.7.0 has no
known wrong values.

## The order

Each phase is one or more units, each with a blueprint, a prototype measured on the S3, abacus's hidden set and a
release.

| # | Phase | What a student gets | Builds on |
|---|---|---|---|
| 1 | Quadratics with letters (done: CAS 041, accepted at 610c81b, abacus #16833) | `X^2=A` gives both roots, -SQRT(A) and SQRT(A), with the row A≥0 (how two roots and a row share one line is the blueprint's to rule); `A×X^2+B×X+C=0` by the formula, with its rows (A≠0, the discriminant's sign) and its steps | CAS 038, 039 (linear letters), CAS 022 (signs) |
| 2 | Trig integrals (done: CAS 042, accepted at 2977838, abacus #17206; definite integrals of these are CAS 044) | powers and products of SIN and COS: `SIN(X)^2`, `SIN(X)^3×COS(X)^2`, `SIN(2×X)×COS(3×X)` | CAS 006, 017 (table, substitution) |
| 3 | Complex roots past degree 2 (first piece done: CAS 043, x^n = c under --complex, accepted at a938637, abacus #17339) | exact complex roots where a closed form fits the line: biquadratics (`X^4+1`), X^n-1 and X^n+1, cubics with one real root; the rest stays PARTIAL | CAS 005, 007, 020 |
| 4 | Matrices | a matrix typed as text, exact entries (rationals, letters, exact values); +, -, ×, scalar ×, transpose, determinant, inverse, rank, row reduction, A×X=B | canon and expand for entries; the arena |
| 5 | Eigenvalues and eigenvectors | the characteristic polynomial det(A-λ×I), its roots by solve (exact where it factors into pieces solve can do; the rest PARTIAL), and an eigenvector for each root | phase 4, solve, factor, phase 3 |
| 6 | The carried list | the 1.7.0 notes' "Not yet" items, in small units between phases or after | |

Why eigenvalues come right after matrices: an eigenvalue is a root of the characteristic polynomial, and Casimir
already factors and solves polynomials exactly. ~~A 2×2 rotation matrix's eigenvalues are complex, which is why phase 3
comes first.~~ (Struck 2026-11-24, abacus: a 2×2 matrix's complex eigenvalues already work through solve's
`--complex`.) ~~Phase 3 comes first for 3×3 and 4×4 matrices.~~ (Struck 2026-11-24, counsel: characteristic
polynomials are almost never phase 3's shapes, so matrices don't wait on it; see below.) 3×3 and 4×4 eigenvalues are exact when the characteristic
polynomial factors into pieces of degree 2 or less, or into phase 3's shapes. A general quartic, or a cubic with three
irrational real roots, stays PARTIAL at any size limit. Textbook problems are usually built to factor.

## counsel's read (2026-11-24), and what it changes

Michael asked that the plan go to counsel, the house's consulting seat, for further input. Its review read this file,
the README, docs/language.md, docs/canonical-form.md and the 005, 008, 020 and 1.7.0 notes, and none of the C. Its
numbers come from four standard-library Python scripts of its own (textbook methods only, no CAS read or run), so
byte counts are its bracket form and its texts, read as ratios rather than budgets. Checked on 1.7.0's command line:
`X^3=2`, `X^4-4×X^2+2`, `X^4-10×X^2+1`, `X^4+1` (also with `--complex`) and `X^3+X+1` are PARTIAL;
`SQRT(X+3)=X-3` and `2^X=8` are NOT_POLYNOMIAL; `SQRT(8)` simplifies to `SQRT(8)`.

**Within the agreed order, adopted by abacus the same day for the blueprints** (casim sizes each in its blueprint;
abacus reviews and writes it):

- **Matrices don't depend on phase 3.** Built textbook matrices factor into linear and quadratic pieces, which solve
  finishes today; random ones give irreducible cubics, which phase 3 mostly leaves PARTIAL (of 20,000 random 3×3 with
  entries -9 to 9, 8.3% have a characteristic polynomial solve can finish). The order stands on its own merits.
- **Phase 1 adds three things:** a discriminant that is a perfect square (`X^2-(A+B)×X+A×B=0` should print X=A and
  X=B, not SQRT((A-B)^2) with a row that always holds; [[A,B],[B,A]]'s eigenvalues are this case); pure powers u^n=c
  with u linear in the variable (`V=4÷3×π×R^3` for R, `A=P×(1+R)^N` for R, and `X^3=2` with numbers); and a ruling on
  the printed form, as CAS 005 ruled it for numeric roots.
- **Phase 2's check needs trig equality decided first.** integrate differentiates its answer and compares exactly, and
  ∫SIN(X)^2's derivative equals SIN(X)^2 only through an identity. A small normal form decides it: everything in SIN
  and COS of the base angle, SIN^2 replaced by 1-COS^2, so each expression is p(COS)+SIN×q(COS) one way. It would also
  read `SIN(A)^2+COS(A)^2-1` as 0, which CAS 040 refuses by its point test.
- **Phase 3, cut to what earns its place, in this order:** (1) X^n=c's real roots (`X^3=2` would be about 26 B; cube
  roots of positives only, a negative c's odd root written `-2^(1÷3)`); (2) biquadratics, real roots first, denested
  where p^2-q^2×r is a rational square (`X^4-10×X^2+1` gives ±SQRT(2)±SQRT(3)), then `X^4+1` and `X^4-X^2+1`;
  (3) X^n=c's complex roots at n = 3, 4 and 6 (X^3±1, X^4-1 and X^6-1 already work with `--complex`). Optional:
  Cardano's one real root (about 63 B for X^3+X+1; all three roots are far past 191 B). Left PARTIAL: n = 5 and past
  unless a COS and SIN form fits (below), and three real irrational roots (a symmetric matrix's
  irreducible cubic is always this case).
- **Phase 4:** a wide checked integer for the work, Bareiss elimination for numbers and cofactors for determinants
  (no division, so it works with letters); the refusal rule stated on the answer ("an answer whose every number is
  below 2^62 is printed"), so a hidden set can count a refused answer that fits; the inverse printed as an integer
  matrix over one positive number (counsel's 4×4 two-digit sample: entry by entry over 191 B in 1,997 of 2,000, as
  integers over one number in 0); letters only where no division or zero test is needed (+, -, ×, transpose, the
  determinant, the inverse as cofactors over the determinant after CAS 040's point test), and perhaps only to 3×3;
  rank, RREF and A×X=B with numbers first; singular and inconsistent as statuses, not refusals; matrices kept out of
  canon, which sorts factors; the work area in the arena (a 4×4 of 64-byte integers is the whole 1 KiB frame allowance).
- **Phase 5:** eigenvectors for a surd or complex eigenvalue in Q[λ]/(m), m the quadratic factor solve finds, every
  entry u+w×λ with u and w rational: (b, λ-a) for a 2×2, a column of adj(λ×I-A) for larger, checked (A-λ×I)×v=0
  exactly before printing (counsel verified 6,000 built matrices this way). One eigenvalue's answer per page, the
  count of independent eigenvectors stated (fewer than the multiplicity means not diagonalizable), a repeated quadratic
  factor refused, and an eigenvector normalized so its first nonzero entry is 1. A hidden set judges eigenvectors by
  property, since they aren't unique, and the form separately, and takes counsel's other cases: Jordan blocks,
  repeated eigenvalues, eigenvalue 0, quadratic and cubic companion blocks, small random matrices beside built
  ones, and refusals counted against the stated rule.

**abacus's ruling on phase 3's cosine form:** `COS(2×π÷5)+SIN(2×π÷5)×0i1` is an exact answer under the guarantee, on
three conditions. (a) It prints in canon's own form: an angle the exact-value table doesn't hold (CAS 008 rule 2)
stays as written, and multiples of π/6 and π/4 reduce through the table as now; π/5 radicals in the table would
change simplify everywhere, so they'd be a unit of their own. (b) It's checked before printing: built as
c^(1÷n)×(COS(2kπ÷n)+SIN(2kπ÷n)×0i1) with k/n reduced, exact by de Moivre, and read back into the polynomial on the
core at CAS 008 rule 6's 1E-24, a miss refusing. (c) It prints only where the whole line fits, else 2 CPLX or
PARTIAL as now. Viète's form and Cardano's complex pair stay PARTIAL.

**New items, on the roadmap by Michael's word, their places still to rank** (abacus: new content, so his to rank.
Michael, 2026-11-24: "I like the additions and new items." Ranked by him the same day on T99 (ask A63, "As
recommended"): radicals in simplify after phase 1, inequalities after phase 2, vectors with phase 4, and the refused
equations after phase 5.
counsel's ranking below is its judgment of Algebra 1 to a first linear algebra course, not a syllabus count):

1. **The equations solve refuses** (NOT_POLYNOMIAL): the variable in a denominator (`1÷X+1÷(X-1)=2`), under a root
   (`SQRT(X+3)=X-3`), in ABS (`ABS(2×X-1)=5`), in one EXP or LN (`2^X=8`, `LN(X)+LN(X-1)=LN(2)`) or one trig layer
   (`2×SIN(X)^2-SIN(X)-1=0`). Each reduces to a polynomial and checks every candidate in the typed equation, which is
   what drops an extraneous root. Against: the calculator's numeric SOLVE answers these near a guess, trig needs a
   notation for a family of solutions, and an irrational candidate can be shown unequal but not equal by the sign
   bound, so these are the hardest to finish honestly.
2. **Inequalities:** linear, quadratic and polynomial, as solve's roots plus the sign between them (CAS 022).
3. **Radicals in simplify:** `SQRT(8)` stays `SQRT(8)` and nothing rationalizes a denominator. Phases 1, 3 and 5 all
   print surds, so this sets their form too.
4. **Vectors** (dot, cross, norm, projection), almost free after phase 4.
5. **Differential equations** (constant coefficients) and **nonlinear systems**, last if at all.

## The long horizon

Michael, 2026-11-24: "I wouldn't mind streaming to ode and pde one day or even laplace transforms too, linear
algebra, etc." None of these is sized or ordered yet. Each would get a blueprint and a measured prototype like any
phase, and the 191-byte line will decide how much of each a calculator can show. What each builds on:

- **Laplace transforms:** a table of transforms and their rules (shift, derivative, convolution), and the inverse
  transform through partial fractions, which Casimir has (CAS 012). Complex roots (phase 3) give the sine and cosine
  terms.
- **Ordinary differential equations:** linear with constant coefficients first, by the characteristic polynomial's
  roots (solve, phase 3) and a template, or by Laplace with initial values; each answer checked by derive put back in
  the equation, as integrate checks itself. First-order separable and linear ones by integrate.
- **Partial differential equations:** the textbook separable cases (heat, wave and Laplace's equation on simple
  regions), as a series solution with its coefficients from defint. Furthest out: their answers are long, and most
  won't fit one line, so this is likely steps and pages, or the command line and web build only.
- **More linear algebra, after phase 5:** diagonalization (P and D), orthogonality and Gram-Schmidt, projections and
  least squares, null space and column space as bases, and vectors (item 4 above).

## The four questions (decided by Michael, 2026-11-24, each as recommended)

1. **The order above,** or matrices and eigenvalues first.
2. **Code room.** Phases 1 to 3 should fit in the 30 KB left. Phases 4 and 5 likely don't. Raise the line per unit
   on its measured size, as now, or set a matrix allowance up front.
3. **Matrices on the calculator.** The firmware has no matrix type today (only its 2 and 3 equation solvers). Casimir
   can do matrices in the library and on the command line first, with the calculator's entry and display screens as a
   firmware unit after; or both together.
4. **Matrix size:** up to 4×4 (a characteristic polynomial of degree 4, and a printed answer that has a chance of fitting
   191 bytes), or larger.

## Housekeeping, not on the critical path

- The host frame table (tests/frames/gcc-15.2.0-x86_64-linux-gnu.su) predates CAS 035. Re-record it before phase 1, as
  its own commit naming the frames that moved by more than 16 B (abacus, 2026-11-24).
- The branches `sums` and `frac-print` are merged history; delete them or keep them (Michael's call).
