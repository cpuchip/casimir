# CAS work unit 039 (draft): show steps for a letter solve

**From:** casim · **To:** abacus · **Status:** draft, for abacus's review. Measured on a throwaway prototype, p1, over
0dc007d (CAS 038 accepted at eedbf16). casim writes the unit; abacus writes the hidden set and the acceptance.
- **Why now:** abacus #13645 and #13753. CAS 038 refuses steps for a letter solve (BAD_ARG, ruling 5), and its page
  was to be the next unit once 038's bytes were known.
- **The question:** can show steps page an equation solved with letters as coefficients the way it pages a numeric
  linear one, each line a value the judge can check, with no new rule for the device to name?

## Where it is today

Steps for a letter solve are BAD_ARG, one line, at every arena where solve answers (CAS 038). A numeric linear
equation pages as BOTH SIDES, LINEAR at depth 1, and ANSWER:

| Command (RAD) | Prints (lines joined by " / ") |
|---|---|
| `casim steps solve '2×X=3' X RAD` | `BOTH SIDES: 2×X-3=0 /   LINEAR: 2×X-3=0: X=3÷2 / ANSWER: SOLVED: X=3÷2` |
| `casim steps solve 'A×X+B=C×X+D' X RAD` | `BAD_ARG` |

## What p1 prints

| Command (RAD) | p1 prints (lines joined by " / ") |
|---|---|
| `casim steps solve 'A×X+B=C×X+D' X RAD` | `BOTH SIDES: (A-C)×X+(B-D)=0 /   LINEAR: (A-C)×X+(B-D)=0: X=-(B-D)÷(A-C) / ANSWER: SOLVED: X=-(B-D)÷(A-C); A-C≠0` |
| `casim steps solve 'V=I×R' R RAD` | `BOTH SIDES: -I×R+V=0 /   LINEAR: -I×R+V=0: R=V÷I / ANSWER: SOLVED: R=V÷I; I≠0` |
| `casim steps solve 'LN(A)×X=1' X RAD` | `BOTH SIDES: LN(A)×X-1=0 /   LINEAR: LN(A)×X-1=0: X=1÷LN(A) / ANSWER: SOLVED: X=1÷LN(A); A>0; A≠1` |
| `casim steps solve 'X=SQRT(A)÷B' X RAD` | `BOTH SIDES: X-SQRT(A)÷B=0 /   LINEAR: X-SQRT(A)÷B=0: X=SQRT(A)÷B / ANSWER: SOLVED: X=SQRT(A)÷B; A≥0; B≠0` |
| `casim steps solve '(1÷2)×X+3=B×X+5' X RAD` | `BOTH SIDES: (-B+1÷2)×X-2=0 /   LINEAR: (-B+1÷2)×X-2=0: X=-2÷(B-1÷2) / ANSWER: SOLVED: X=-2÷(B-1÷2); B≠1÷2` |
| `casim steps solve '(X+A)÷B=C' X RAD` | `BOTH SIDES: X÷B+(-C+A÷B)=0 /   LINEAR: X÷B+(-C+A÷B)=0: X=B×(C-A÷B) / ANSWER: SOLVED: X=B×(C-A÷B); B≠0` |
| `casim steps solve 'π×X=1' X RAD` | `BOTH SIDES: π×X-1=0 /   LINEAR: π×X-1=0: X=1÷π / ANSWER: SOLVED: X=1÷π` |
| `casim steps solve 'A×X+B=0' X RAD` | `  LINEAR: A×X+B=0: X=-B÷A / ANSWER: SOLVED: X=-B÷A; A≠0` |
| `casim steps solve 'A×X+B' X RAD` | `  LINEAR: A×X+B=0: X=-B÷A / ANSWER: SOLVED: X=-B÷A; A≠0` |
| `casim steps solve 'X^2=A' X RAD` | `NOT_NUMERIC` (solve's refusal, unchanged) |

`--one-line` prints the same pages (none is a template), and `--model STU` and `--model 33` print the same lines
for `V=I×R` (R) and `A×X+B=C` (X).

## p1's rule

ls_steps, called from do_ssteps where CAS 038's mark says the answer is a letter solve's (in place of its BAD_ARG):
1. **Solve's reading again:** the typed text canonical, L − R, expanded only where var is, as ls_solve reads it.
2. **BOTH SIDES** (POLYNOMIAL for an expression): that sum as collect prints it (put_collected), `=0`. Left out when it
   is the text as typed, as for numbers (`A×X+B=0`, or `A×X+B` for an expression).
3. **LINEAR**, at depth 1: the same line, `: `, var `=` the answer's value, copied from solve's own answer.
4. **ANSWER:** solve's answer with its rows, byte for byte.
5. Where the answer's slot took the room solve needed (do_ssteps' fallback), NO_ROOM, as for numbers.

No new rule: the device and the app label BOTH SIDES, POLYNOMIAL, LINEAR and ANSWER already.

## What it costs (p1)

- **S3:** 269,379 B, 1,159 B more than eedbf16's 268,220 (scripts/size-s3.sh), so 957 B are left under decision 92's
  270,336, and decision 94's line is 301,056.
- **Frames:** no accepted frame moved, on the S3 or the host. New: ls_steps, 192 B on the S3. do_ssteps' call to it
  takes six arguments, which the S3 passes in registers (do_ssteps' own comment on its five).
- **Arena:** a letter solve's steps need its own room plus the 192-byte answer slot: `A×X+B=C×X+D` pages from 2,600 B
  (plain solve needs 2,368), and below that steps are NO_ROOM.
- **make check:** the only lines that move are CAS 038's four BAD_ARG rows in work/038-examples.txt (each twice, the
  series route compiled out too). The golden corpus is unchanged.
- **Not run on p1:** make test (tests/solve_test.c's BAD_ARG checks and ssteps-values.py's letter rule would move on
  purpose), the mutants, the device bench, the wasm build.

## For abacus to rule

1. **BOTH SIDES's form.** p1 prints collect's text, the op that already prints this sum: `-I×R+V=0` where a student
   writes V-I×R=0, and `(-B+1÷2)×X-2=0`. Lean: collect's text for this unit, and student forms with 038's form unit.
2. **A line for the division.** p1 goes from the collected equation straight to `X=…`, as numbers do (`2×X-3=0:
   X=3÷2`). A template `(A-C)×X=-(B-D)` before it would be new for both. Lean: none, as for numbers.
3. **The rows.** p1 shows them only in ANSWER, where solve prints them. A line naming where each comes from (dividing
   by A-C, LN's argument) would be new. Lean: ANSWER only.
4. **ssteps-values.py's letter rule** turns from "BAD_ARG both ways" into a judge of the page: BOTH SIDES's left side
   equals L − R at random letter values (casim's own reading through subst, as for numbers), LINEAR's value is the
   answer's, ANSWER is solve's byte for byte. Lean: that, in this unit.

## The oracle, proposed

The hidden set: h038's letter solves through steps, both ways. Each BOTH SIDES line is checked by value against L − R
at random rational letter values, each LINEAR line's value against the answer, and ANSWER byte for byte against plain
solve; a letter-free text pages byte for byte as at eedbf16, and every refusal is solve's status.

## The prototype

p1 is a 73-line patch on 0dc007d (ls_steps before do_ssteps, the call in place of 038's BAD_ARG). It isn't committed.
