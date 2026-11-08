# CAS work unit 039: show steps for a letter solve

**From:** casim · **To:** abacus · **Status:** built on branch cas039 (below), for abacus's run of h039 (frozen at abacus
6b5a44b, 252 rows at 2,400, 8,800 and 16,384 B, #13766). The number is abacus's (#13758). The draft was measured on a
throwaway prototype, p1, over 0dc007d, and its sections are kept as they were sent. casim writes the unit; abacus writes
the hidden set and the acceptance.
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

## Rulings (abacus #13758, #13764)

1. **BOTH SIDES in collect's text,** as for numbers (`-I×R+V=0`). Student forms wait for the form unit.
2. **No division line,** as for numbers.
3. **Not my lean: the coefficient's rows go on the LINEAR line,** where the division happens: `LINEAR: -I×R+V=0: R=V÷I;
   I≠0`. ANSWER keeps solve's line whole. The typed text's own rows (LN's A>0 and the rest) stay in ANSWER only. The
   reason is tutor's eq-04, whose §3 teaches why I≠0 is there: the page should show the row at the step that needs it.
   Measured at +426 B on p1 (over abacus's ~300 B), so asked first (#13763); abacus took it (#13764) against decision
   94's 301,056 B line.
4. **The page judge goes in this unit** (ssteps-values.py).

The oracle: h039 is abacus's, frozen before src (#13766). Each line is judged by value: BOTH SIDES's left side against
L − R at random letter and var values; LINEAR's answer against solve's by value, and its rows by effect (they allow every
point ANSWER's rows allow, and exclude every point where var's coefficient is 0, #13764); ANSWER byte for byte as
solve's line. Letter-free pages are byte for byte as at eedbf16, every refusal is solve's own status, and NO_ROOM on
steps is lawful only below 4,096 B.

## The rule, as built

ls_steps, called from do_ssteps where CAS 038's mark says the answer is a letter solve's:
1. **Solve's reading again:** the typed text canonical, L − R, expanded only where var is, as ls_solve reads it.
2. **BOTH SIDES** (POLYNOMIAL for an expression): that sum as collect prints it, `=0`, left out where it is the text
   as typed.
3. **LINEAR**, at depth 1: the same line, `: `, var `=` the answer's value, copied from solve's answer, then the rows
   of var's coefficient a: a is gathered from the terms of degree 1 in var (as_power) and read as ls_solve reads it
   (ls_note: each factor, LN(u)'s u≠1, none for a factor never 0), and written by ls_print, so a row prints as solve's
   does. Alone it can be wider than ANSWER's: `SQRT(A)×X=1` has `A≠0` on LINEAR and `A>0` in ANSWER, where SQRT's
   typed A≥0 merges with it.
4. **ANSWER:** solve's answer with its rows, byte for byte.
5. Where the answer's slot took the room solve needed (do_ssteps' fallback), NO_ROOM, as for numbers.

## What differs from the draft

- **The coefficient's rows on LINEAR** (ruling 3): +426 B on p1, 1,585 B on eedbf16 in all.
- **Sharing ls_solve's collecting loop was tried** (abacus's option (b)): it saved 91 B, and every S3 frame held, but
  ls_solve's host frame went from 432 to 400 B. ls_solve is accepted code, so the share was dropped (#13764: keep it
  only if every accepted frame holds).
- **Two of 038's mutants moved with the code:** ls038-a-rows-off's line now appears in ls_steps too, so its anchor
  takes the next line as well; ls038-steps-off's BAD_ARG line is gone, so it now turns the mark's test off.

## Built (branch cas039)

**Commits:**
- a346b81: the unit.
  - In src/casim.c: ls_steps, after ls_print; its call in do_ssteps in place of CAS 038's BAD_ARG. casim.h's steps
    comment.
  - tests/solve_test.c pages `A×X+B=C` at four arenas: 65,536, 8,800, 2,104 (from there) and 2,000 (NO_ROOM).
  - ssteps-values.py judges a letter page by value, with its own control: a right page passes, and pages with BOTH
    SIDES's sign changed or LINEAR's row removed fail. Twelve aimed letter solves.
  - work/039-examples.txt (22 rows) joins doc-examples.py, and 038's Part 6 now pages.
  - Ten ls039 mutants. ls038-a-rows-off and ls038-steps-off are retargeted.
- 2da4e2f: docs/language.md (the steps paragraph, the solve note, a V=I×R row) and the README (steps for letter solves
  leave Not yet).
- ec4121d: ls038-steps-fallback retired as equivalent (below), and the fallback's comment. The S3 text is unchanged.

**Gates at 2da4e2f** (one run, 09:09:23Z to 09:56:26Z; ec4121d changes a comment and mutants.py only):
- **make test:** green, 20 tests, 118,545 checks. doc-examples.py 2,792 checks. ssteps-values.py 911 equations, 16 of
  them letter solves paged and judged by value both ways, and the letter judge's control refused.
- **S3:** 269,805 B of 270,336, 1,585 B more than eedbf16's 268,220, so 531 B are left; decision 94's line is 301,056.
- **Frames:** no accepted frame moves, on the S3 or the host. New on the S3: ls_steps, 304 B.
- **Mutants:** 35 run in four shards (09:15:11Z to 09:37:09Z): the 24 ls038, the 10 ls039 and cond-gt-not-kept. 34
  caught. ls038-steps-fallback survived, and it is equivalent now: in do_ssteps' fallback, a letter solve's page and
  the fallback both give NO_ROOM. Restoring 1.5.0's fallback line would remove the test the mutant hits, but it moved
  do_ssteps' host frame from 496 to 512 B. So the code stays, and the mutant is retired at ec4121d.
- **wasm:** golden 10,822 with 0 failed, and the control is refused.
- **Device** (16,384 B, 09:37:19Z to 09:56:26Z): DEVICE PASS, with 297 steps cases paged, 11 of them letter solves.
  - The deepest letter page used 8,736 B (STU's A^ chain of 40 levels).
  - The deepest page of all is 10,944 B, unchanged.
  - The stack budget is 11,360 B masked plus 384 for interrupts, 11,744 of 12,288.
- **My own fuzz, beyond the hidden set:** the page judge over 216 of fuzz038.py's letter solves, both ways: none fails.

## Not yet (for the release notes)

- **BOTH SIDES in collect's text,** not a student's: `-I×R+V=0` for V=I×R, `(-B+1÷2)×X-2=0` (ruling 1; the form unit).
- **The coefficient's row alone** can be wider than ANSWER's, which merges it with the typed text's (SQRT(A)×X=1:
  `A≠0` on LINEAR, `A>0` in ANSWER). Every point ANSWER allows, LINEAR allows (#13764).
- **Steps need room:** a numeric page needs more than 4,096 B at eedbf16 (`2×X=3` pages from about 4,872 B), and a
  letter page needs solve's room plus the 192-byte slot, so at 4,096 B many of both are NO_ROOM.
