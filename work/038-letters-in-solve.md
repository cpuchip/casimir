# CAS work unit 038: letters as coefficients in solve

**From:** casim · **To:** abacus · **Status:** built on branch cas038 (below), for abacus's run of h038 (frozen at abacus
61237b8, 225 rows at 2 arenas, #13635). The number is abacus's (#13632). The draft was measured on a throwaway
prototype, p1, over 3f165a0 (1.5.0's code), and its sections are kept as they were sent. casim writes the unit; abacus
writes the hidden set and the Accepted section.
- **Why now:** Michael, 2026-11-08, after 1.5.0: "I was going to have you work on whatever is good/next for casimir."
  casim's pick from the README's "doesn't do (yet)" list and 1.5.0's notes ("letters as coefficients in solve"): a
  student is more likely to rearrange a formula than to meet one of the Not yet rows.
- **The question:** for an equation linear in the unknown whose coefficients hold other letters (`V=I×R` for R), can
  solve print the unknown exactly, with the rows where that answer holds?

## Where it is today

pw reads the typed polynomial with rational coefficients, and any letter besides var stops it: NOT_NUMERIC ("another
letter", docs/language.md's `A×X^2-1`). So every formula a student rearranges is refused.

| Command (RAD) | 1.5.0 prints | p1 prints |
|---|---|---|
| `casim solve 'V=I×R' R RAD` | `NOT_NUMERIC` | `SOLVED: R=V÷I; I≠0` |
| `casim solve 'P×V=N×R×T' T RAD` | `NOT_NUMERIC` | `SOLVED: T=P×V÷N÷R; N≠0; R≠0` |
| `casim solve 'A×X+B=C' X RAD` | `NOT_NUMERIC` | `SOLVED: X=-(B-C)÷A; A≠0` |
| `casim solve 'Y=M×X+B' M RAD` | `NOT_NUMERIC` | `SOLVED: M=-(B-Y)÷X; X≠0` |
| `casim solve '(X+A)÷B=C' X RAD` | `NOT_NUMERIC` | `SOLVED: X=B×(C-A÷B); B≠0` |
| `casim solve 'X÷A=B' X RAD` | `NOT_NUMERIC` | `SOLVED: X=A×B; A≠0` |
| `casim solve 'X×Y÷Y=3' X RAD` | `NOT_NUMERIC` | `SOLVED: X=3; Y≠0` |
| `casim solve 'X×A=X' X RAD` | `NOT_NUMERIC` | `SOLVED: X=0; A≠1` |
| `casim solve '(A^2-1)×X=A+1' X RAD` | `NOT_NUMERIC` | `SOLVED: X=(A+1)÷(A^2-1); A≠-1; A≠1` |
| `casim solve 'X×(A+B)=A^2-B^2' X RAD` | `NOT_NUMERIC` | `SOLVED: X=(A^2-B^2)÷(A+B); A+B≠0` |
| `casim solve 'LN(A)×X=1' X RAD` | `NOT_NUMERIC` | `SOLVED: X=1÷LN(A); A>0; LN(A)≠0` |
| `casim solve 'C=2×π×R' R RAD` | `NOT_NUMERIC` | `SOLVED: R=C÷2÷π` |
| `casim solve 'A=X' X RAD` | `NOT_NUMERIC` | `SOLVED: X=A` |
| `casim solve 'SQRT(A)×X=1' X RAD` | `NOT_NUMERIC` | `NOT_NUMERIC` (a u≥0 row, not in p1) |
| `casim solve 'A×(X-1)=A×X' X RAD` | `NOT_NUMERIC` | `NOT_NUMERIC` (X drops out) |
| `casim solve 'X^2=A' X RAD` | `NOT_NUMERIC` | `NOT_NUMERIC` (degree 2) |
| `casim solve 'A×SIN(X)=B' X RAD` | `NOT_NUMERIC` | `NOT_NUMERIC` (X inside a function) |

At A = −1 in `(A^2-1)×X=A+1` every X solves, and at A = 1 none does, so both rows are the ones the answer needs. At
A = 1 in `X×A=X` every X solves.

## p1's rule

ls_solve, called where pw refused NOT_NUMERIC; when it declines, pw's refusal stands as 1.5.0 prints it.
1. **Only a text with a letter besides var, as typed** (raw_letter, each side of the =). A letter-free text keeps pw's
   answer: `X÷0=1` stays NOT_NUMERIC. p1's first build read the letter after canon, which had already refused the ÷0
   as UNDEFINED, and the golden corpus caught it on 3 lines (10128, 10286, 10287).
2. **L − R as a×var + b,** canonical and expanded only where var is (collect's expansion), each term of degree 0 or 1
   in var with a coefficient free of it (as_power). Anything else declines.
3. **a canonically 0 declines** (var drops out).
4. **var = −b÷a, canonical, checked:** put back into L − R and expanded fully, it must be 0, or ls_solve declines.
5. **The rows:**
   - a≠0, unless a is never 0 (td_never0: 1÷u, EXP, a nonzero number), written as cd_ne writes CAS 023's rows (each
     letter factor, a power's base, k×L^m + c0 solved for L);
   - each condition of the typed text (CAS 035's td_walk, on each side of the =), read at var = the answer: u≠0, u>0,
     LN's u≠1 and TAN's COS(u)≠0 as rows. A letter-free one that fails is UNDEFINED, and one whose sign isn't known
     declines. u≥0, u≥1, ABS(u)≤1 and ABS(u)<1 decline in p1 (cdset keeps only ≠ and >).

## What it costs (p1)

- **S3:** 267,091 B, 1,897 B more than 3f165a0's 265,194 (scripts/size-s3.sh, both runs today), so 3,245 B are left
  under decision 92's 270,336.
- **Frames:** no accepted frame moved, on the S3 or the host.
  - New: ls_solve 288 B and ls_end 32 B on the S3, and 448 and 8 on the host.
  - put_solution.constprop's clone is now put_solution, at the same 400 B.
  - p1's first build moved do_solve by 16 B (S3 288 → 304, host 400 → 416). Moving the hook's tail into ls_end put it
    back.
- **The host checks:** make check green on p1. Every golden line is unchanged (10,822 checked, 0 failed), and the
  frame table is green.
- **Not run on p1:** make test, the device bench, the wasm build and the mutants.

## What p1 doesn't do, and one defect

- **Steps are wrong on p1.** `casim steps solve 'A×X+B=C' X RAD` prints `BOTH SIDES: 0=0`, then the right answer. The
  step printer reads solve's rational coefficients, which ls_solve never fills in. The unit has to give these steps
  their own page (L − R as a×X + b, then the answer) or refuse steps for them. p1 does neither.
- **Not in p1:** a u≥0 row (SQRT), the case where var drops out, degree 2 and up with letters, var inside a function,
  and `--complex`, which is unchanged.

## For abacus to rule

1. **The answer's form.** p1 prints canon's text, which is simplify's: `-(B-C)÷A`, `B×(C-A÷B)`, `C÷2÷π`,
   `(A^2-B^2)÷(A+B)`.
   - (a) canon's text, as every other op prints;
   - (b) the shorter of canon's and the fully expanded text (`-A+B×C` for the second);
   - (c) cancel's reduced form, with its rows (`A-B; A+B≠0` for the last).

   Lean (a) for this unit. (b) and (c) aren't measured.
2. **`LN(A)≠0` beside `A>0`.** Lean: write it A≠1, as CAS 035's 'n' kind does for a divisor LN(u).
3. **When var drops out** (`A×(X-1)=A×X`), lean: keep 1.5.0's NOT_NUMERIC for this unit. A status that says "no
   solution unless a row fails" would be new.
4. **SQRT's u≥0 row.** Lean: add ≥ to cdset so `X=SQRT(A)` prints `SOLVED: X=SQRT(A); A≥0`. Its cost isn't measured.
5. **Steps:** a page of their own, or a refusal (above). Lean: the page, if it fits in the bytes left. Its cost isn't
   measured.

## The oracle, proposed

The hidden set: formulas linear in one letter, with letters, π and the typed functions as coefficients, on both sides
of the =. Each answer is checked outside casim, by putting it back at random rational letter values. Each row is checked
by a point where it fails: there, the equation has every value or none, or no value as typed. The guarantee holds as
for every op: no answer without its check, and no wrong value.

## The prototype

p1 is a 121-line patch on 3f165a0 (ls_solve and ls_end after cd_print, the hook in do_solve, two forward declarations).
It isn't committed. casim sends it on request.

## Rulings (abacus #13632, #13645)

1. **The answer's form:** (a), canon's text, as every op prints. A form unit across all ops can come later.
2. **LN(A)≠0 is written A≠1,** so `LN(A)×X=1` has the rows `A>0; A≠1`.
3. **var dropping out stays NOT_NUMERIC.** docs/language.md's NOT_NUMERIC now says what is still refused.
4. **≥ is added,** for SQRT's u≥0 and ACOSH's u≥1. ABS(u)≤1 and ABS(u)<1 still decline. Every golden line stays.
5. **Steps are refused** for a letter solve, with the status steps gives an op it doesn't show (BAD_ARG). Their page is
   CAS 039, after this unit (#13645).
6. **The π gate,** measured: a gate of "a letter or π as typed" moves 0 of the corpus's 10,822 lines, so π is let in.
   The other suites hold no π solve row that moves: work/016-examples.txt's three are poles, which stay UNDEFINED.
   tests/solve_test.c's `π×X-1` moves from NOT_NUMERIC to `SOLVED: X=1÷π`, on purpose.
7. **The budget:** 038 fits under decision 92's 270,336 B (below). Decision 94 (abacus b0f27fc) lets the line rise to
   301,056 B, and 038 doesn't use it.

The oracle as proposed: each answer is put back at random rational letter values, and each row is checked at a point
where it fails. h038 also reads steps on any letter solve as one status line, var dropping out as exactly NOT_NUMERIC,
and a letter-free text as 1.5.0 prints it, solve and steps byte for byte (#13635).

## The rule, as built

ls_solve runs where pw refused NOT_NUMERIC. When it declines, pw's refusal stands as 1.5.0 prints it.
1. **Only a text with a letter besides var, or π, as typed.** A text with neither keeps pw's answer: `X÷0=1` and
   `SIN(1)×X=1` stay NOT_NUMERIC.
2. **L − R as a×var + b,** canonical and expanded only where var is, each term of degree 0 or 1 in var with a coefficient
   free of it. Anything else declines, and so does a text holding model 33's variable i (as a sum refuses it).
3. **a canonically 0 declines** (var drops out).
4. **var = −b÷a, canonical, checked:** put back into L − R and expanded fully, it must be 0, or ls_solve declines.
5. **The conditions,** each kept once:
   - a≠0, read as the typed conditions read a divisor (td_canon): each factor and a power's base, LN(u)'s u≠1, and none
     for a factor never 0 (EXP, a number, 1÷u);
   - each condition of the typed text (CAS 035, on each side of the =), read at var = the answer: u≠0, u>0, u≥0 (SQRT),
     u≥1 (ACOSH), LN's u≠1 and TAN's COS(u)≠0. A u≠0 is split into its factors again once the answer is in it.
   - u≠0 and u≥0 on one u are kept as u>0.
6. **A condition with no letter:** one that holds is left out. One that fails is UNDEFINED where it has no var as typed
   (`A×X=LN(-1)`, as sum, limit and defint read it). Where it holds var, the one answer is where the text has no value
   and the text has a value elsewhere, so ls_solve declines (`X×(X-A)÷(X-A)=A`). One whose sign isn't known declines.
7. **The rows** are written by CAS 035's td_rows, each solved for its letter where it is linear in one or even in one
   (`ABS(A)≥1`). A ≥ or ≤ row beside a ≠ row of the same sides is > or <: `A≠0` and `A≤0` are `A<0`.
8. **Show steps:** the answer marks do_solve's first coefficient (q = 0, which pw never leaves there), and do_ssteps
   returns BAD_ARG for it, before the steps take their room, and after the fallback where only plain solve fits.

## What differs from the draft

- **The rows go through td_rows, not cd_print.** Every variant with a ≥ branch in cd_print moved its S3 frame from 288
  to 272 B (six variants compiled), and cd_print without it stays at 288. So ls_print is its own, and CAS 023's rows
  (limit's and taylor's) are untouched. cd_note's merge reads `op != S->op[i]`, which is the same for its two old ops,
  and cond-gt-not-kept's anchor moved with it.
- **a≠0 is read by td_canon,** so `LN(A)×X=1` is `A>0; A≠1` (ruling 2), and `SQRT(A)×X=1` answers `A>0` (p1 declined).
- **A failing condition holding var declines.** On a build between p1 and this one, `X×(X-A)÷(X-A)=A` printed
  UNDEFINED, which says the text has no value, and it has one at every X but A.
- **i declines.** p1 answered `i×X=A` with `X=A÷i`.
- **The do_solve hook passes no new argument.** Passing cr moved do_solve's frame by 16 B on the S3 and the host, so
  ls_solve marks unit 2, cr's place.
- **Steps at small arenas.** Between 1,984 and 3,200 B a letter solve's steps were NO_ROOM, because the mark was read
  after the steps reserved their room. The mark is read first now.

## Built (branch cas038)

**Commits:**
- b02ac06: the unit. ls_solve, ls_note, ls_print and ls_end in src/casim.c, the hook in do_solve, the mark and its
  BAD_ARG in do_ssteps, cd_note's merge; casim.h's NOT_NUMERIC and steps comments; tests/solve_test.c's letters()
  (20 rows exactly, each answer put back on the core, each row on one letter checked at its edge, 10 declines, steps
  at four arenas), and its two rows that move on purpose (`Y×X-1`, `π×X-1`); work/038-examples.txt (54 rows) in
  doc-examples.py; 23 ls038 mutants, and cond-gt-not-kept's anchor.
- 0c3276e: docs/language.md (the paragraph, the NOT_NUMERIC line, six rows, the ÷0 note, the steps note) and the
  README; ssteps-values.py reads a letter solve's steps as BAD_ARG, and fails when no input is one.
- 9ec4ac8: scripts/measure.py, 42 bench rows of letter solves at depth, 34 to 47 levels.
- e39f0b6: two rows with model 33's i in b, in letters() and the examples (56 rows), for ls038-i-off (below).

**Gates at 9ec4ac8** (one run, 06:53:33Z to 07:34:18Z):
- **make test**, which runs make check first: green, 20 tests, 118,524 checks (06:53:43Z to 06:58:44Z). doc-examples.py
  2,726 checks at 9ec4ac8 and 2,730 at e39f0b6; ssteps-values.py 900 equations, 5 of them letter solves.
- **S3** (scripts/size-s3.sh): 267,944 B of decision 92's 270,336, 2,750 B more than 1.5.0's 265,194, so 2,392 B are
  left.
- **Frames:** no accepted frame moves on the S3 or the host. New on the S3: ls_solve 272, ls_note 176, ls_print 160 and
  ls_end 32. put_solution.constprop's clone is put_solution at the same 400 B, as on p1. The host table holds, with
  the four new functions unrecorded, as CAS 034 to 037 left theirs.
- **Mutants:** 24 run (the 23 ls038 and cond-gt-not-kept) in four shards, 06:58:44Z to 07:14:55Z: 23 caught.
  ls038-i-off survived. `i×X=A` declines without the i test too, because a = i is a factor with no letter and no known
  sign. With i in b only (`X×A=i`, `X=i×A`) the mutant answers `X=i÷A; A≠0`, so e39f0b6 adds those two rows. Rerun
  at e39f0b6 (07:34Z to 07:36Z), it is caught by work/038-examples.txt:83 and :84, so all 24 are caught.
  ls038-check-off-deg2 is caught by the golden corpus's `A×X^2-1` (lines 598, 599, 1656).
- **wasm** (scripts/wasm.sh): the golden corpus through the module, 10,822 lines with 0 failed, and its control
  refused. The letter rows through it under node print as the native build prints them.
- **Device** (scripts/device.sh at 16,384 B, 07:15:05Z to 07:34:18Z): DEVICE PASS, 1,454 cases equal to the host's.
  The worst stack is unchanged, 11,360 B masked (STU's X^ chain of 44 levels), plus 400 for interrupts, 11,760 of
  12,288. The deepest of the 42 new letter solves is 8,208 B (STU's A^ chain of 40 levels, answered).

## Not yet (for the release notes)

- **canon's forms a student wouldn't write:** `X=B×(C-A÷B); B≠0` for (X+A)÷B=C, `-(B-C)÷A`, `C÷2÷π` (ruling 1).
- **ACOSH's zero is not read as LN's is:** `ACOSH(A)×X=1` is `A≥1; ACOSH(A)≠0`, which says A>1.
- **A row on an odd power** is written as it is: `(A^3-1)×X=1` has `A^3-1≠0`, where CAS 023's rows would write A≠1.
- **No ABS(u)≤1 or ABS(u)<1 row:** ASIN, ACOS and ATANH of a letter decline.
- **pw's first refusal decides** whether letters are read: `X^2÷X×A=1` stops at ÷X (NOT_POLYNOMIAL), and `A×X^2÷X=1`
  answers.
- **A constant with neither a letter nor π,** `SIN(1)×X=1` or `SQRT(2)×X=1`, stays NOT_NUMERIC.
- **Past six conditions** (CD_MAX), NO_ROOM: `A×B×C×D×E×F×G×X=1`.
- **Degree 2 or more with letters, var inside a function, var dropping out:** NOT_NUMERIC.
- **Steps** for these: CAS 039.
- **The device:** calc.c:3903 hands casim_run the typed text, so once the pin moves the device's SOLVE answers letter
  equations. Its rows view is soroban's to check then (#13632).
