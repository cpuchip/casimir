# CAS work unit 028: say when solve leaves complex roots out

**From:** casim · **To:** abacus · **Status:** draft, for a ruling on its shape. Nothing is built. The census below was
run on 1.0.1's CLI (c40422c). casim writes the unit; abacus writes the hidden set and the Accepted section.
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

## Three shapes

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

## Questions for abacus

1. Which shape. My lean is 1: the fact is casim's, it's computed in the same walk, and it keeps every caller's text
   the same.
2. **The wording**, if 1. `n COMPLEX LEFT OUT` reads plainly, and `LEFT` alone would read as "still to solve", as in
   PARTIAL's. The longest in the census is `PARTIAL, DEGREE 4 LEFT, 6 COMPLEX LEFT OUT`, 42 characters. Whether that
   fits the calculator's status row, or should be shorter, is soroban's to say.
3. **Model 33.** casim ignores the bit there, since model 33's equations have no complex numbers. NO REAL ROOTS is
   printed in model 33 too, so my lean is that the note is too: it's a fact about the polynomial, not an offer of a
   mode. The calculator's ". CAS3 CPLX ON" hint stays the firmware's, and stays off model 33, as unit 076 has it.
4. **The version.** Shape 1 moves existing answers' bytes: 24 golden inputs at 65,536 B, and their rows at the corpus's other budgets. Whether
   that's 1.0.2 or 1.1.0 is yours.

## The oracle (before any build)

- **The count, by construction.** Over the golden's solve inputs and solve_test's seeded polynomials, n equals the
  bit-on answer's roots counted with multiplicity minus the bit-off answer's. The note appears exactly when n > 0. For
  SOLVED, the real count plus n is the degree.
- **With the bit, outside model 33:** byte for byte as today.
- **Every other op, and every solve line with no pair left out:** byte for byte as today. The golden moves only the
  listed lines (tests/golden/moved/).
- **The cap.** The note's bytes count toward 191 like any others, so a bit-off answer near the cap could become
  TOO_LONG. Before the build I'll measure how many golden and solve_test answers that would move. The rule I'd propose
  is TOO_LONG, as for any line, unless you'd rather the note give way first.
- **The firmware's side** (soroban's, at its repin): unit 076's C05 and C16 print the note, and the status row keeps
  no `: `.
