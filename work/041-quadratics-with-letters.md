# CAS work unit 041: quadratics and pure powers with letters

**From:** casim · **To:** abacus · **Status:** DRAFT, 2026-11-24, measured on a throwaway prototype, p1, over f81ddc0
(1.7.0's code). The number is the roadmap's proposed one until abacus writes the unit. casim writes the unit; abacus
writes the hidden set and the acceptance.

- **Why now:** phase 1 of the roadmap (work/roadmap.md), agreed with Michael 2026-11-24, with counsel's two additions
  adopted by abacus (#16650): the perfect-square discriminant and pure powers.
- **The question:** for an equation of degree 2 in the unknown, or a pure power of it, whose coefficients hold other
  letters, can solve print the real roots exactly, with the rows where they hold, and print a perfect square's roots
  without a root sign?

## Where it is today

CAS 038 reads L − R as a×var + b; a term of degree 2 or more declines, and pw's NOT_NUMERIC stands.

| Command (RAD) | 1.7.0 prints | p1 prints |
|---|---|---|
| `casim solve 'X^2=A' X RAD` | `NOT_NUMERIC` | `SOLVED: X=-SQRT(4×A)÷2; X=SQRT(4×A)÷2; A≥0` |
| `casim solve 'A×X^2+B×X+C=0' X RAD` | `NOT_NUMERIC` | `SOLVED: X=-(B+SQRT(B^2-4×A×C))÷2÷A; X=-(B-SQRT(B^2-4×A×C))÷2÷A; A≠0; B^2-4×A×C≥0` |
| `casim solve 'X^2-(A+B)×X+A×B=0' X RAD` | `NOT_NUMERIC` | `SOLVED: X=B; X=A` |
| `casim solve 'X^2-2×A×X+A^2-B^2=0' X RAD` | `NOT_NUMERIC` | `SOLVED: X=A-B; X=A+B` |
| `casim solve 'X^2+2×A×X+A^2=0' X RAD` | `NOT_NUMERIC` | `SOLVED: X=-A (MULT 2)` |
| `casim solve '4×X^2-12×A×X+9×A^2=0' X RAD` | `NOT_NUMERIC` | `SOLVED: X=3×A÷2 (MULT 2)` |
| `casim solve 'X^2-X×(A+1)+A=0' X RAD` | `NOT_NUMERIC` | `SOLVED: X=1; X=A` |
| `casim solve 'X^2+X=A' X RAD` | `NOT_NUMERIC` | `SOLVED: X=-SQRT(4×A+1)÷2-1÷2; X=SQRT(4×A+1)÷2-1÷2; A≥-1÷4` |
| `casim solve '(X-A)^2=B' X RAD` | `NOT_NUMERIC` | `SOLVED: X=A-SQRT(4×B)÷2; X=A+SQRT(4×B)÷2; B≥0` |
| `casim solve 'S=1÷2×G×T^2' T RAD` | `NOT_NUMERIC` | `SOLVED: T=SQRT(2×G×S)÷G; T=-SQRT(2×G×S)÷G; G≠0; 2×G×S≥0` |
| `casim solve 'E=M×C^2' C RAD` | `NOT_NUMERIC` | `SOLVED: C=SQRT(4×E×M)÷2÷M; C=-SQRT(4×E×M)÷2÷M; M≠0; 4×E×M≥0` |
| `casim solve 'V=4÷3×π×R^3' R RAD` | `NOT_NUMERIC` | `SOLVED: R=(3×V÷4÷π)^(1÷3); 3×V÷4÷π≥0` |
| `casim solve 'X^4=A' X RAD` | `NOT_NUMERIC` | `SOLVED: X=-A^(1÷4); X=A^(1÷4); A≥0` |
| `casim solve 'X^3+A=0' X RAD` | `NOT_NUMERIC` | `SOLVED: X=(-A)^(1÷3); A≤0` |
| `casim solve 'A×X^3=8' X RAD` | `NOT_NUMERIC` | `SOLVED: X=(8÷A)^(1÷3); A≠0; 8÷A≥0` |
| `casim solve 'X^5=32×A^5' X RAD` | `NOT_NUMERIC` | `SOLVED: X=(32×A^5)^(1÷5); 32×A^5≥0` |
| `casim solve 'X^2+1+A^2=0' X RAD` | `NOT_NUMERIC` | `SOLVED: X=-SQRT(-4×A^2-4)÷2; X=SQRT(-4×A^2-4)÷2; -4×A^2-4≥0` (**wrong: the row never holds**) |
| `casim solve 'X^3=2' X RAD` | `PARTIAL, DEGREE 3 LEFT` | `PARTIAL, DEGREE 3 LEFT` (numeric solve; p1 doesn't reach it) |
| `casim solve 'X^2=4' X RAD` | `SOLVED: X=-2; X=2` | the same (numeric solve) |

Every root p1 prints was put back into a×x^2 + b×x + k (or a×x^n + k) and expanded to 0 before printing. canon folds
SQRT(u)^2 to u, and (u^(1÷n))^n to u, so the put-back closes on each row above.

## p1's rule

Inside ls_solve, after it reads L − R term by term in var (CAS 038's reading, collect's expansion):
1. **Degrees 0, 1 and 2 only, with a degree 2 coefficient a not canonically 0:** the formula (lq_quad).
2. **Degrees 0 and n only, 3 ≤ n ≤ 12:** the pure power (lq_pow). Any other mix declines, and pw's refusal stands.
3. **a read 0 by CAS 040's six points refuses** NOT_NUMERIC, as for the linear case.
4. **The discriminant** D = b^2 − 4×a×k, canonical and fully expanded. A number below 0 declines in p1.
5. **The perfect square (lq_sqrt):** where D is an expanded polynomial in letters with rational coefficients, its square
   root by the long-division method (the leading term by lex order over A to Z: its coefficient a rational square and
   every exponent even, then each next term the remainder's leading term over twice the root's). A remainder of 0 gives
   s, and the roots are (−b ∓ s)÷(2×a) with no D row; s = 0 is one root, MULT 2. Anything else (a function, π, a
   remainder that never closes in 24 steps) uses SQRT(D) with the row D≥0.
6. **The pure power:** r = −k÷a. Even n: ±r^(1÷n) with r≥0. Odd n: r^(1÷n) with r≥0, since the calculator gives a
   negative number's (1÷3) power a complex value; a number below 0 prints −(−r)^(1÷n) with no row. An even n with r a
   number below 0 declines in p1.
7. **Rows:** a≠0 as CAS 038 reads it; D≥0 or r≥0; each typed condition read at each root. A typed condition that fails
   at one root declines the whole answer in p1.
8. **Print:** `SOLVED: X=x1; X=x2`, then the rows, as CAS 038 prints them.

## What it costs (p1)

- **S3:** 275,648 B, 5,238 B more than 1.7.0's 270,410 (scripts/size-s3.sh), 25,408 B under decision 94's 301,056.
- **Frames:** no recorded frame moved, on the host (the table re-recorded at 6312839) or the S3 (every row of the S3
  table diffed against the last S3 table on disk, from 2026-11-15, which matches 1.7.0's but for ls_zero, whose 96 B
  the 1.7.0 notes give and p1 keeps). New on the S3: lq_sqrt 400 B, lq_quad 304, lq_pow 160, lq_mono 64. lq_sqrt runs
  expand_fully and canon inside its own frame, so the worst stack needs the device bench; not run.
- **make check:** green but 3 golden lines, all `solve A×X^2-1` (lines 598, 599, 1656), which move from NOT_NUMERIC to
  `SOLVED: X=-SQRT(4×A)÷2÷A; X=SQRT(4×A)÷2÷A; A>0`. That is the unit's purpose: docs/language.md gives `A×X^2-1` as
  NOT_NUMERIC's example of another letter. Not run on p1: make test, the device bench, the wasm build and the mutants.

## What p1 gets wrong, and what it leaves out

1. **A row that never holds.** `X^2+1+A^2=0` prints two roots with `-4×A^2-4≥0`, false for every A. This is CAS 040's
   class (an answer whose row never holds) in a new place.
2. **A square left under the root.** `SQRT(4×A)÷2` is SQRT(A), and `SQRT(32×A)÷2` is 2×SQRT(2×A). canon doesn't take a
   square factor out of SQRT (CAS 008 rule 3), and the formula's 4×a×k puts one in.
3. **Root order.** p1 prints (−b − r)÷(2×a) first. When a's sign flips the text (`S=1÷2×G×T^2` gives the + root first),
   and a perfect square prints `X=B; X=A`, by the root's lex sign.
4. **Rows not solved for their letter:** `3×V÷4÷π≥0` is V≥0, `8÷A≥0` beside A≠0 is A>0, `32×A^5≥0` is A≥0.
5. **A perfect power left as a power:** `(32×A^5)^(1÷5)` is 2×A.
6. **Not in p1:** numeric pure powers (`X^3=2` stays PARTIAL; numeric solve answers before ls_solve runs), a pure power
   of a linear u (`(X+1)^3=A`, which expand turns into a full cubic), a letter exponent (`A=P×(1+R)^N`), `--complex`,
   steps, and a typed condition that holds at one root only.

## For abacus to rule

1. **The answer's form.** CAS 038 ruled canon's text. Lean: canon's text, with the discriminant's square content taken
   out of the root inside solve (its numeric content's largest square and each letter's even power), so `X^2=A` prints
   `X=-SQRT(A); X=SQRT(A); A≥0`. simplify's own radical rule stays the roadmap's candidate item, unchanged here.
2. **Root order.** Lean: the two roots in canon's order (cmp), so the text doesn't depend on how the equation was typed
   or on a's sign. A perfect square then prints `X=A; X=B`.
3. **A D row that never holds.** Lean: read D at CAS 040's six points; where it reads below 0 at every point that has a
   value, refuse NOT_NUMERIC (no value is taken from digits, so "no real roots" isn't printed for letters). Where −D is
   a perfect square (D = −s^2), refuse the same way. A D row that holds everywhere (`A^2+1≥0`) stays, a row for nothing
   but never a wrong value, unless CAS 022's sign reading shows it.
4. **D a number below 0** (letters only in a, with a≠0): lean `NO REAL ROOTS`, as numeric solve prints, with a's rows.
5. **Pure powers.** Lean: as p1 (r^(1÷n) with r≥0 for odd n with letters; −(−r)^(1÷n) for a number). A monomial
   perfect power's root taken exactly by the same method as the square (`X^5=32×A^5` gives `X=2×A`), for odd n only. An even n's root of a perfect power is an ABS (`X^4=A^4` would be `X=-ABS(A); X=ABS(A)`): lean
   leave it as r^(1÷n) in this unit.
6. **Rows solved for the letter:** divide a ≥ or ≤ row by a positive constant factor (a number, π) before td_rows reads
   it. Lean yes; this may move CAS 038's rows too, and would be measured against h038 and h039.
7. **Numeric pure powers in this unit or the next.** `X^3=2` needs a hook where numeric solve prints PARTIAL for
   x^n − c. Lean: in this unit, since abacus listed it (#16650); its cost isn't measured.
8. **Out of this unit:** a pure power of a linear u, a letter exponent, `--complex` with letters, and steps (refused
   BAD_ARG as CAS 038 refused them, the page a follow-on as CAS 039 was). A typed condition that fails at one root:
   lean print the other root alone.

## The oracle, proposed

Built equations and drawn ones, both judged outside casim:
- **Built:** (p×X − q)×(r×X − t) = 0 with p, q, r, t polynomials in letters, expanded: the answer must print without a
  root sign, and its roots must equal q÷p and t÷r at random rational letter values. a×X^2 + b×X + k with random
  polynomial coefficients: each printed root put back at random rational letter values where the rows hold, and every
  row checked at a point where it fails (there the equation has no real root, every value, or no value as typed).
- **No row that never holds:** each printed row sampled at letter values until it holds once; a row that never holds in
  the sample is a failure, the class p1 gets wrong.
- **Pure powers:** a×X^n + k for n 3 to 12, the same put-back and row checks.
- **Unchanged:** every letter-free text and every linear letter solve prints as at 1.7.0, solve and steps byte for byte,
  but the `A×X^2-1` rows.

## The prototype

p1 is a patch on f81ddc0 in a throwaway worktree: lq_isqrt, lq_mono, lq_sqrt, lq_pow and lq_quad before ls_solve, and
the degree reading in ls_solve. It isn't committed. casim sends it on request.
