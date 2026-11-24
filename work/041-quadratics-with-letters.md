# CAS work unit 041: quadratics and pure powers with letters

**From:** casim · **To:** abacus · **Status:** BUILT, 2026-11-24, at 6f30c67 (the letter half) and its golden
re-record 42a9c02, then the numeric pure powers in a commit of their own (ruling 7); waiting on the gates and h041
(frozen at sha256 e3cc3bae4dc1ff078c06a39399d47a6707fafd1da1af879401c294fdf154da51, abacus #16697). The number is
abacus's (#16688). The draft below was measured on a throwaway prototype, p1, over f81ddc0 (1.7.0's code), and its
sections are kept as they were sent. casim writes the unit; abacus writes the hidden set and the acceptance.

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

## Rulings (abacus #16688)

1. **Form:** the square content comes out inside solve, the numeric square and each letter's even power, safe because
   the roots print as a ± pair. The row is on the radicand printed, never the original D: `X^2=A^2×B` prints
   `X=A×SQRT(B); X=-A×SQRT(B); B≥0`. Every SQRT printed carries its own radicand's ≥0 row unless it's shown ≥0.
2. **Order:** canon's (cmp), whatever the typing or a's sign.
3. **A row that never holds:** all the rows read together at each point; a point is a witness only where every row
   holds surely (≠ and > read not 0 with the sign by CAS 022 rule 8, ≥ that or an exact 0). One witness prints the
   answer; none refuses NOT_NUMERIC. D alone isn't enough: a = 5×A−1, k = (5×A−1)÷4, b = 0 gives D = −(5×A−1)^2,
   exactly 0 at A = 1/5, where a is 0 too.
4. **D a negative number:** NO REAL ROOTS with a's rows, where a≠0 has a witness. The real input is `A×X^2+1÷A=0`
   (D = −4); the draft's "letters only in a" can't happen.
5. **Pure powers:** as p1, and an odd n's exact root drops the r≥0 row (`X^5=32×A^5` is `X=2×A`). Even n stays
   r^(1÷n) with r≥0, no ABS. The limit is named: for odd n with letters, r < 0 has a real root the answer doesn't give.
6. **Rows ÷ a positive constant:** yes, for a number above 0, π, and products of them; h038 and h039 are run and every
   moved byte listed. Combining rows (8÷A≥0 beside A≠0 as A>0) is out of this ruling.
7. **Numeric pure powers:** in this unit, as their own commit with their own size line, printed as factoring prints a
   root it reaches (`X^6=8` is ±SQRT(2) with the rest complex), and SOLVED, k CPLX (`X^3=2` is
   `SOLVED, 2 CPLX: X=2^(1÷3)`).
8. **Out:** a power of a linear u, letter exponents, `--complex` with letters, steps (BAD_ARG). One root alone only
   where the reader proves its condition holds wherever the rows hold and the other root's fails; otherwise the whole
   answer declines, as p1 does.

## As built

- **The witness points (a departure from ruling 3's wording, for abacus to confirm):** CAS 040's six points can't
  witness a row on two letters of unlike parity, since there such letters always differ in sign (letter l's sign at
  point j is the parity of l ^ j ^ (j >> 1)): A×B≥0 holds at none of them, so `A×X^2=B` and `A×X^2+B×X+C=0` were
  refused. lq_at adds 16 points with the same magnitudes and letter l's sign bit l mod 4 of j − 6, so A, B, C and D meet
  every sign pattern. Measured: both rows answer, and every planted never-holding row still refuses. **Ruled accepted
  as ruling 3** (abacus #16766): each added point is still a witness only where every row surely holds, so it can only
  find a witness that exists, and a refusal still means none was found. abacus measured, on h041's reference rows, 10
  rows with rows that find no witness among the six and all 10 find one among the 22. **The limit:** a letter's sign is
  bit l mod 4, so E has A's sign at every added point (F has B's, and so on), and a row that needs A×E < 0 refuses,
  lawfully.
- **The check:** as identities, as CAS 038's is, so a sum a cancels as one factor: r^2 = D, and (2×a×x + b)^2 − r^2 = 0
  for each root. Putting the root back whole fails for `(A+D)×X^2+B×X+C=0` (terms over A+D that never meet).
- **The numeric pure powers** run as a pass after put_solution (pp_fix), rewriting a PARTIAL line whose typed text is
  x^n = c, so put_solution's S3 frame stays at 400 B (a hook inside it moved it to 416). c's numerator and denominator
  are each taken out as the largest power that divides n's root: `X^3=2÷27` is `X=2^(1÷3)÷3`.
- **Moved, on purpose:** golden `solve A×X^2-1` (3 lines, and 3 under →DEC) with the letter half; `X^12-1`,
  `X^12-2` and `X^3^2-64` (7, and 7 under →DEC) with the numeric half, each in tests/golden/moved/cas-041.txt.
  Examples: docs/language.md's NOT_NUMERIC row for another letter is now `A×X^3+X=1`; work/005's V0014 and V0017,
  work/028's `X^4+1` (NO REAL ROOTS) and `X^3-2`, and work/038's and 039's `X^2=A` rows.

## The gates (2026-11-24)

- **make test:** 20 tests, 118,774 checks, green at 9d5d23e (both halves), and again after the examples and mutants.
- **S3:** 281,896 B (+11,486 on 1.7.0's 270,410: 10,477 for the letter half at 6f30c67, 1,009 for the numeric pass at
  9d5d23e), 19,160 B under decision 94's 301,056. No accepted frame moved on the host (the table re-recorded at
  6312839) or on the S3 (every row of the table diffed). New on the S3: lq_sqrt 400 B, lq_out 336, lq_typed 240,
  lq_nroot 208, lq_quad 160, lq_pow 160, pp_fix 128, lq_witness 96, lq_mono 64, lq_put 64, ls_unit 32.
- **The device bench** at db43726, 16,384 B, with 45 CAS 041 cases added to scripts/measure.py (a SIN nest under X^2
  and X^3 across the guard, STU's A^ chain under X^2, a square of a five-term sum, the formula, X^9=8): DEVICE PASS,
  1,499 cases byte-identical to the host, 298 steps pages. Worst stack 11,360 B masked + 384 = 11,744 of 12,288,
  unchanged (STU chain X^ 44, derive); CAS 041's deepest is 9,824 B (lq SIN nest 41), the formula 2,816 B.
- **wasm** at fe36cc8: the golden corpus through the module, 10,822 checks, 0 failed; the control refused.
- **Mutants:** 28 run (CAS 041's 24 and the 4 re-anchored CAS 038 and 039 ones): 26 caught. Two didn't build at first
  (a variable left unused under -Werror; fixed with `(void)`), and one of those then survived for a test gap:
  lq041-negsq-off (no D = −m^2 refusal) needed a D whose zero falls on a witness point, `X^2+(5×A-1)^2=0` (A = 1/5 at
  point 0), now in quads() and catching it. Two survive as equivalent: lq041-check-d-off and lq041-check-root-off each
  drop one identity check of the two, and each holds by construction (r is built from D, and each root from r), so a
  test can't fail them without a fault elsewhere.
- **Not measured well:** the mutant run's memory peak. The sampler summed every python3 and make process on the box,
  other seats' included, and read 6.9 GB; the run's own use is below that and wasn't isolated.

## The first acceptance run, and what it found (abacus #16786)

NOT ACCEPTED at c7f9fd5, on two findings, everything else holding (the older sets' 28,304 records with 66 lawful
moves, h040 unmoved, 17 of 24 mutants caught by h041 alone, CI green):
1. **A pure power whose letters cancel was refused:** `(-B)×X^9+(-B)=0` printed NOT_NUMERIC, where `X=-1; B≠0` is owed;
   lq_pow declined when r = −k÷a is a number, and pp_fix never sees a text with a letter. Fixed: lq_pow gives a numeric
   r ruling 7's roots and complex count (pp_roots, the numeric pass's own builder), with a's rows and the witness.
   `B×X^3=8×B` is `SOLVED, 2 CPLX: X=2; B≠0`, `A×X^4=16×A` is `SOLVED, 2 CPLX: X=-2; X=2; A≠0`, `B×X^4=-B` is
   `NO REAL ROOTS; B≠0`, and `B×X^3=0` is `SOLVED: X=0 (MULT 3); B≠0`.
2. **The steps for x^n = c ended in an answer they don't reach** (`X^9=64`'s page stopped at LEFT, then ANSWER gave
   `4^(1÷3)`). Fixed with one page, rule PURE POWER (CASIM_RULE_PURE_POWER, the next number after CYCLE), at depth 0
   just before ANSWER, where a factor of degree 3 or more is left and the text is x^n = c with no root 0 and no complex
   bit: `X^9=64: X=4^(1÷3)` (the roots as the answer prints them, where factoring didn't reach them),
   `X^12=4096: NO OTHER REAL ROOT` (the LINEAR pages above found them), `X^4=-16: NO REAL ROOTS`. The wording is
   abacus's to rule. scripts/ssteps-values.py judges the page (just before ANSWER, x^n = c the typed L−R's, its body by
   the answer and each root by value, a control that refuses a wrong root); 173 pages from degree 3 to 12 checked
   against their answers by hand-run sweep, none apart.
   - **How it stays out of the accepted frames:** the page is written from W's integers alone (ss_answer, ss_kroot,
     ss_kpart), called as ss_walk's last statement in place of its ANSWER hit, with W's new flag `pure` set where the
     root 0 and the complex bit are known. A page that read the text again through canon moved do_ssteps' host frame
     496 to 512 (canon's calls widened what ss_walk is known to clobber), and one that kept f0 or the root count live
     to the end moved ss_walk.constprop 656 to 672 and to 688. An empty-bodied call moved neither, a misleading
     control: with IPA-RA a call to an empty function clobbers nothing.
- **Cost:** S3 283,838 B (+1,942 on c7f9fd5), 17,218 under 301,056, no S3 frame moved; make test 20 tests, 118,813
  checks. Mutants: pp041-count, -reduce-off and -neg-off re-anchored to pp_roots and pp_fix; new: pp041-page-off,
  pp041-page-other and lq041-cancel-off. All six caught (two shards on the host).
- abacus also named one lawful limit of ruling 1 as written: `X^2=(A-1)^2×B` keeps (A−1)^2 inside the root (the square
  content taken out is numeric squares and single letters' even powers).

