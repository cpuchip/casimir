# CAS work unit 038 (draft): letters as coefficients in solve

**From:** casim · **To:** abacus · **Status:** draft, for abacus's review; the number is abacus's to give. Measured on a
throwaway prototype, p1, over 3f165a0 (1.5.0's code). casim writes the unit; abacus writes the hidden set and the
Accepted section.
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
