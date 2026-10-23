# CAS work unit 028: say when solve leaves complex roots out

**From:** casim · **To:** abacus · **Status:** revised after abacus's ruling (#10137): shape 1, the wording as drafted,
model 33 too, 1.1.0. The cap isn't 0, so its cases are back with abacus before any build (§The cap). h028 is frozen
after this revision. Nothing is built. Every measurement here was run on 1.0.1's CLI (c40422c). casim writes the unit;
abacus writes the hidden set and the Accepted section.
- **From:** soroban #9798 item 8, unit 076's "Not in this unit" (stu32-firmware work/076-complex-roots.md). abacus
  put it first on casim's post-1.0 list (#9803).
- **The question:** with the complex bit off, when a quadratic factor with no real root sits beside real roots, does
  solve's answer say a root was left out?

## Where it is today

With the bit off, a quadratic factor with no real root gives nothing to the answer line. When it's the only factor,
the line says so: `X^2+1` is `NO REAL ROOTS`, and the calculator adds "CAS3 CPLX ON" (unit 076). When there are real
roots beside it, the line says nothing:

| input, bit off | today | with `--complex` |
|---|---|---|
| `(X-2)×(X^2+1)` | `SOLVED: X=2` | `SOLVED: X=2; X=-0i1; X=0i1` |
| `(X^2+1)^2×(X-3)` | `SOLVED: X=3` | `SOLVED: X=3; X=-0i1 (MULT 2); X=0i1 (MULT 2)` |
| `(X^2+1)×(X^3-2×X-5)` | `PARTIAL, DEGREE 3 LEFT` | `PARTIAL, DEGREE 3 LEFT: X=-0i1; X=0i1` |

docs/language.md calls solve's answer "real roots", so SOLVED isn't wrong. But a student who reads `SOLVED: X=2` for a
cubic has no sign that two roots exist and aren't shown. Show steps does show it, since the factor's page says
`QUADRATIC FORMULA: X^2+1=0: NO REAL ROOTS`; the answer line doesn't.

**Census** over the golden's solve inputs at 65,536 B, 1.0.1's CLI, each run with the bit off and on (model 33's read
as 35, since it ignores the bit). Of 168 inputs in models 35 and STU, 21 leave roots out without a word:
- 20 SOLVED: 19 leave 2 out, and 1 leaves 4;
- 1 PARTIAL, `X^12-1`, which leaves 6 out.

5 more say NO REAL ROOTS already. Model 33 has 3 more of the silent kind.

## Three shapes (ruled: 1)

1. **casim's line says it, in the status (my lean).** With the bit off, when quadratic factors with no real root were
   left out, the status gains `, n COMPLEX LEFT OUT`. n counts the roots left out with multiplicity, so for SOLVED the
   real roots' count plus n is the degree.
   - `SOLVED, 2 COMPLEX LEFT OUT: X=2`
   - `SOLVED, 4 COMPLEX LEFT OUT: X=3`
   - `PARTIAL, DEGREE 3 LEFT, 2 COMPLEX LEFT OUT`

   The note goes before the first `: `, which the calculator's report splits on, so the root rows are untouched (unit
   076's lesson). It reads like `PARTIAL, DEGREE n LEFT`. There's no interface change: casim.h is 1.0's. Show steps'
   ANSWER line is solve's line, as today.
2. **A flag beside the line.** casim reports the count out of band, for example as a count casim_run fills in, and
   the calculator words it. That keeps every answer's bytes, but changes the interface, so it's a minor version, and the
   wording moves into each caller.
3. **No casim change.** With CPLX off, the calculator calls solve a second time with the bit, and compares. This costs
   a second solve on the device each time, and the count logic moves into the firmware.

## Questions for abacus (ruled below)

1. Which shape. My lean is 1: the fact is casim's, it's computed in the same walk, and it keeps every caller's text
   the same.
2. **The wording**, if 1. `n COMPLEX LEFT OUT` reads plainly, and `LEFT` alone would read as "still to solve", as in
   PARTIAL's. The longest in the census is `PARTIAL, DEGREE 4 LEFT, 6 COMPLEX LEFT OUT`, 42 characters. Whether that
   fits the calculator's status row, or should be shorter, is soroban's to say.
3. **Model 33.** casim ignores the bit there, since model 33's equations have no complex numbers. NO REAL ROOTS is
   printed in model 33 too, so my lean is that the note is too: it's a fact about the polynomial, not an offer of a
   mode. The calculator's ". CAS3 CPLX ON" hint stays the firmware's, and stays off model 33, as unit 076 has it.
4. **The version.** Shape 1 moves existing answers' bytes: ~~24 golden inputs at 65,536 B, and their rows at the
   corpus's other budgets~~ 84 lines in each of corpus.txt and decimal.txt (§The golden). The struck count was of the
   rows at 65,536 B only, and the corpus has solve inputs at other budgets alone. Whether that's 1.0.2 or 1.1.0 is
   yours.

## The oracle (before any build)

- **The count, by construction.** Over the golden's solve inputs and solve_test's seeded polynomials, n equals the
  bit-on answer's roots counted with multiplicity minus the bit-off answer's. The note appears ~~exactly when n > 0~~ on
  a SOLVED or PARTIAL line exactly when n > 0. NO REAL ROOTS keeps its text: `X^4+4` has n = 4 and stays `NO REAL
  ROOTS`. For SOLVED, the real count plus n is the degree.
- **With the bit, outside model 33:** byte for byte as today.
- **Every other op, and every solve line with no pair left out:** byte for byte as today. The golden moves only the
  listed lines (tests/golden/moved/).
- **The cap.** The note's bytes count toward 191 like any others, so a bit-off answer near the cap could become
  TOO_LONG. Before the build I'll measure how many golden and solve_test answers that would move. ~~The rule I'd
  propose is TOO_LONG, as for any line, unless you'd rather the note give way first.~~ Ruled (#10137 (5)): no answer
  that fits 191 today becomes TOO_LONG. Measured in §The cap.
- **The firmware's side** (soroban's, at its repin): unit 076's C05 and C16 ~~print the note~~ and 048's S13 print
  the note, and the status row keeps no `: ` (§Readers).

## Ruled (abacus #10137)

1. **Shape 1,** with n from every route where the bit drops a root, each named with an input (§The routes). abacus
   covers each route in h028.
2. **`n COMPLEX LEFT OUT`,** after a comma in the status. Whether it fits the status row is soroban's (asked, #10143);
   if it doesn't, the shorter form is soroban's.
3. **Model 33: yes,** as NO REAL ROOTS. The CPLX hint stays the firmware's, off model 33.
4. **1.1.0.** casim.h doesn't change, but a caller comparing the status to `SOLVED` exactly breaks. Michael hears it
   with the release.
5. **No answer that fits 191 today becomes TOO_LONG.** Measure first; if it isn't 0, the cases come back before any
   build.
6. **Every reader of solve's status** listed before the build (§Readers). The firmware's part goes to soroban at the
   repin.

## The routes

The bit is read in two places in src/casim.c, both through `quad_roots`. One is put_solution, the answer line (line
4799). The other is the steps walk's own call (line 5175). NM_CX is set from solve_flags (line 4866), which drops the
bit in model 33, and start() strips it from every angle inside casim, so an internal solve never has it. `quad_roots`
drops a root in one branch: `if (d < 0 && !cx) return true;`, a quadratic factor with a negative discriminant. There is
no cubic or quartic formula and no X^n=c route. solve finds linear and quadratic factors over Q (factors()), and a
factor of degree 3 or more is left whole, with the bit or without it. So every root the bit can print comes from a
quadratic factor, and n is twice the multiplicity of each such factor.

| route | input | bit off today | with the note |
|---|---|---|---|
| a no-real-root quadratic beside real roots | `(X-2)×(X^2+1)` | `SOLVED: X=2` | `SOLVED, 2 COMPLEX LEFT OUT: X=2` |
| the same, repeated | `(X^2+1)^2×(X-3)` | `SOLVED: X=3` | `SOLVED, 4 COMPLEX LEFT OUT: X=3` |
| several such factors | `X^6-1` | `SOLVED: X=-1; X=1` | `SOLVED, 4 COMPLEX LEFT OUT: X=-1; X=1` |
| X^n=c, through factors() | `X^3-8` | `SOLVED: X=2` | `SOLVED, 2 COMPLEX LEFT OUT: X=2` |
| beside PARTIAL's factor | `(X^2+1)×(X^3-2×X-5)` | `PARTIAL, DEGREE 3 LEFT` | `PARTIAL, DEGREE 3 LEFT, 2 COMPLEX LEFT OUT` |
| model 33, bit on or off | `(X-2)×(X^2+1)` | `SOLVED: X=2` | `SOLVED, 2 COMPLEX LEFT OUT: X=2` |
| steps' ANSWER (the root 0 split off first) | `X^4+X^2=0` | `ANSWER: SOLVED: X=0 (MULT 2)` | `ANSWER: SOLVED, 2 COMPLEX LEFT OUT: X=0 (MULT 2)` |
| no real root at all | `X^4+4` | `NO REAL ROOTS` | as today |
| a factor of degree ≥ 3 left whole | `X^4+1`, `X^3-2` | `PARTIAL, DEGREE 4 LEFT` | as today (n = 0: the bit prints none of its roots either) |
| an internal solve (integrate's and partfrac's hole rows, ir_hole) | | read for its first 13 bytes, `NO REAL ROOTS` | as today: NO REAL ROOTS has no note, and any other line is a hole either way |

Not a route: quad_roots' exact-check failure (`return false`) counts a factor as unsolved, `f->n += 2 * mult`, so it
reads as PARTIAL's degree left, not as complex roots. abacus #5077 found no input that reaches it.

## The golden

84 solve lines move in each of tests/golden/corpus.txt and tests/golden/decimal.txt: 44 in model 35, 17 in STU and 23
in model 33, from 77 distinct inputs. That's every OK line whose answer is SOLVED or PARTIAL and whose input, run with
the bit, has n > 0. The other 569 solve lines, and every other op's, stay byte for byte.

## The cap (brought back, #10137 (5))

Measured with ~/scratch/casim/c29/cap028.py, which isn't part of the repository. For each bit-off answer that fits
today, it adds the note's bytes. n is computed two ways and asserted equal wherever both exist: the bit-on count minus
the bit-off count, and the degree (from expand) minus the real roots minus PARTIAL's degree left.
- **The golden: 0.** 77 inputs get the note, and the longest of them is 40 B today.
- **A family built to reach the cap: not 0.** Degree 12: one or two quadratics with no real root, the rest real
  quadratics with irrational roots (ax^2+bx+c, a to 9, b and c to ±9) and linear factors, in models 35 and STU.

  | seed | draws | get a note | fit today, past 191 with it |
  |---|---|---|---|
  | 1 | 1,500 | 1,416 | 210 |
  | 2 | 1,500 | 1,403 | 213 |

  Each is today's answer at 172 to 191 B. For example:
  - `(4×X^2-7×X-4)×(7×X^2-1×X-5)×(1×X+8)×(X^2-1×X+1)×(6×X^2+4×X-5)×(5×X^2+8×X+2)` is `SOLVED: X=-8;
    X=(-2-SQRT(34))÷6; X=(-4-SQRT(6))÷5; X=(1-SQRT(141))÷14; X=(7-SQRT(113))÷8; X=(-4+SQRT(6))÷5; X=(-2+SQRT(34))÷6;
    X=(1+SQRT(141))÷14; X=(7+SQRT(113))÷8`, 172 B today and 192 B with `, 2 COMPLEX LEFT OUT`.
- **A shorter note doesn't close it.** At seed 1, a 15 B note (`, 2 COMPLEX OUT`) would pass 191 in 159, and a 21 B
  note (n ≥ 10) in 223. Any note adds bytes, so some answer that fits today won't fit with it.
- **The choices, for abacus.** Each meets rule 5:
  - **(a) the note gives way (my lean).** Where the line with the note passes 191, solve prints today's line, byte for
    byte. Those answers stay silent, as in 1.0.1, but none is lost. The rule is checkable: the note is present
    exactly when n > 0 and the noted line fits.
  - **(b) a shorter note first, then (a).** This narrows the silent set, but needs two wordings and two thresholds.
  - **(c) drop roots to make room.** I'd rule it out: a root is the answer, and the note isn't.

## Readers of solve's status

Grepped on lepton, 2026-10-23, for `SOLVED`, `NO REAL ROOTS`, `PARTIAL, DEGREE` (and the tests' `_` spellings).

- **casimir:**
  - src/casim.c: the writer (put_solution; the steps pages' `NO REAL ROOTS`), and ir_hole's 13-byte `NO REAL ROOTS`
    prefix test, which doesn't change (§The routes);
  - src/casim.h: the status comment (lines 78, 209-212), which gets the note;
  - tests/solve_test.c: exact strcmp rows that leave a pair out move with the build: `X^3^2-1` in 35 (X^6-1) and in
    STU (X^9-1, PARTIAL). The prefix tests (`strncmp(out, "SOLVED", 6)`) still hold;
  - scripts/ssteps-values.py: its judge's expected ANSWER lines. readback() skips ANSWER rows, and NO REAL ROOTS pages
    don't change;
  - scripts/mutants.py: the anchors on put_solution's status lines (`f->n >= 3`, cx-no-real-roots-with-bit) move with
    the code;
  - examples/web/index.html and wasm/casim.mjs read the status enum (`r.status === "OK"`) and show out as it is. No
    change;
  - docs/language.md (161-163, the table at 177-183, the steps example at 415), docs/canonical-form.md (353-360, 374)
    and README.md (11, 101). Of the README's two examples, neither leaves a pair out, so they stay. language.md's
    steps example `X^4+X^2=0` moves.
- **stu32-firmware** (soroban's, at the repin):
  - core/calc.c: ab_cas_row splits casim's line at its first `: `, so the note lands in row 0, the status row; the
    roots don't move. The `strcmp(out, "NO REAL ROOTS")` for the CPLX hint doesn't change;
  - tests/076-soroban.txt C05 and C16, and tests/048-soroban.txt S13 (`X^3^2-64` stamped in model 35) print the note.
    app_test's `strstr(..., "SOLVED")` still holds. device/standin's cas2.txt and cplx076.txt have no pair left out;
  - firmware/wasm reads the report through the same ab_cas_row.
- **stu32-tutor,** a shallow clone of 365e2df in scratch: no file names any of the statuses. Its calc/ container passes
  casim's answer through.
- **Not on lepton:** stu32-primer, learn's site. If "the tutor's WASM page" is a /play/ page there, its reader needs a
  grep by a seat with that checkout.
