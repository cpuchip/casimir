# Casimir roadmap after 1.7.0

Status: AGREED, 2026-11-24, by casim at Michael's request, for abacus to amend. Units are numbered when abacus writes
them; the numbers below are the proposed order, not assignments. abacus read it the same day: the order and the four
recommendations stand, with one correction to what phase 3 buys (struck in place below). Michael took all four
recommendations the same day: this order, code room raised per unit on its measured size, matrices in the library
and command line first with the calculator's screens after, and matrices up to 4×4. The plan is with counsel for a
further read before phase 1's blueprint.

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
| 1 | Quadratics with letters | `X^2=A` gives both roots, -SQRT(A) and SQRT(A), with the row A≥0 (how two roots and a row share one line is the blueprint's to rule); `A×X^2+B×X+C=0` by the formula, with its rows (A≠0, the discriminant's sign) and its steps | CAS 038, 039 (linear letters), CAS 022 (signs) |
| 2 | Trig integrals | powers and products of SIN and COS: `SIN(X)^2`, `SIN(X)^3×COS(X)^2`, `SIN(2×X)×COS(3×X)` | CAS 006, 017 (table, substitution) |
| 3 | Complex roots past degree 2 | exact complex roots where a closed form fits the line: biquadratics (`X^4+1`), X^n-1 and X^n+1, cubics with one real root; the rest stays PARTIAL | CAS 005, 007, 020 |
| 4 | Matrices | a matrix typed as text, exact entries (rationals, letters, exact values); +, -, ×, scalar ×, transpose, determinant, inverse, rank, row reduction, A×X=B | canon and expand for entries; the arena |
| 5 | Eigenvalues and eigenvectors | the characteristic polynomial det(A-λ×I), its roots by solve (exact where it factors into pieces solve can do; the rest PARTIAL), and an eigenvector for each root | phase 4, solve, factor, phase 3 |
| 6 | The carried list | the 1.7.0 notes' "Not yet" items, in small units between phases or after | |

Why eigenvalues come right after matrices: an eigenvalue is a root of the characteristic polynomial, and Casimir
already factors and solves polynomials exactly. ~~A 2×2 rotation matrix's eigenvalues are complex, which is why phase 3
comes first.~~ (Struck 2026-11-24, abacus: a 2×2 matrix's complex eigenvalues already work through solve's
`--complex`.) Phase 3 comes first for 3×3 and 4×4 matrices. Their eigenvalues are exact when the characteristic
polynomial factors into pieces of degree 2 or less, or into phase 3's shapes. A general quartic, or a cubic with three
irrational real roots, stays PARTIAL at any size limit. Textbook problems are usually built to factor.

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
