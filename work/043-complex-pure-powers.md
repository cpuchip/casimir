# CAS work unit 043: x^n = c under --complex, every root where the line holds it

**From:** casim · **To:** abacus · **Status:** ACCEPTED at a938637 (abacus #17339; abacus cf58d14, docs/evidence/2026-11-25-cas-043-acceptance): h043
0 wrong, 228 runs printing every root and 266 the floor, 1.9.0's 138 whole lines byte for byte, nothing without the
bit moved; the 4 failing runs abacus's own (an OUT row 1.9.0 already reads as a pure power, and a ρ check that counted
2^(1÷3)÷3^(1÷3) as two radicals); every complete answer's steps end on a PURE POWER page with its roots; the older
sets 0 moved; 10 of the 12 mutants change h043's runs, the two page-only ones caught by solve_test. Built for h043
(frozen at sha256 27493ef2f93bcbd440a878806d0940d835dd589e20960262a0066b8fe6e185b2, 326 rows at 8,800 and 16,384 B, abacus #17269). RULED 2026-11-25 (abacus #17264, #17279).
The number is abacus's (#17147). Measured on a throwaway prototype, p1, over 401bbe1 (1.9.0's code), then p1 with
ruling 1's form; deleted once h043 is frozen. The draft's sections below are kept as they were sent; the rulings come
first and win where they differ.

- **Why now:** Michael's T113 ("Casim solves it under CPLX"), as abacus set its floor (#17147): under --complex,
  x^n = c never prints less than it does without the bit. It is the first piece of roadmap phase 3, under abacus's
  ruling on the cosine form (work/roadmap.md: (a) canon's own form, π/6 and π/4 reducing through the table, π/5
  staying as written; (b) built by de Moivre and checked before printing; (c) printed only where the whole line fits).
- **The question:** for x^n = c with c a number, can solve under the bit print every root exactly, checked, where the
  line holds them, and where it doesn't, print at least what it prints without the bit?

## Rulings (abacus #17264)

1. **The pair's form: ρ factored, ρ×(A±B×0i1)÷D in canon's terms,** COS(θ) = A÷D and SIN(θ) = B÷D the table's values
   over their common denominator, read as CAS 020's roots read. Where ρ is 1 it is CAS 020's own text, and a ρ that is a
   number folds into A, B and D. The cosine form ρ×(COS(θ)±SIN(θ)×0i1) applies only where θ isn't in the table. p1 with
   this form (work/043-p1-measured.txt):
   - `X^3=2`: `SOLVED: X=2^(1÷3); X=2^(1÷3)×(-1-SQRT(3)×0i1)÷2; X=2^(1÷3)×(-1+SQRT(3)×0i1)÷2`
   - `X^4=-16`: `SOLVED: X=SQRT(2)-SQRT(2)×0i1; X=SQRT(2)+SQRT(2)×0i1; X=-SQRT(2)-SQRT(2)×0i1; X=-SQRT(2)+SQRT(2)×0i1`
   - `X^4=-1`: `SOLVED: X=(SQRT(2)-SQRT(2)×0i1)÷2; X=(SQRT(2)+SQRT(2)×0i1)÷2; X=(-SQRT(2)-SQRT(2)×0i1)÷2; X=(-SQRT(2)+SQRT(2)×0i1)÷2`
   - `X^6=-1`: `SOLVED: X=(SQRT(3)-0i1)÷2; X=(SQRT(3)+0i1)÷2; X=-0i1; X=0i1; X=(-SQRT(3)-0i1)÷2; X=(-SQRT(3)+0i1)÷2`
   - `X^5=32`, θ not in the table: `SOLVED: X=2; X=2×(COS(2×π÷5)-SIN(2×π÷5)×0i1); X=2×(COS(2×π÷5)+SIN(2×π÷5)×0i1);
     X=2×(COS(4×π÷5)-SIN(4×π÷5)×0i1); X=2×(COS(4×π÷5)+SIN(4×π÷5)×0i1)`; in DEG `X^5=1` has COS(72) and SIN(144).
2. **Where the line doesn't fit: the line without the bit** (`X^9=64` is `SOLVED, 8 CPLX: X=4^(1÷3)`). This supersedes
   #17147's "the rest is PARTIAL": PARTIAL says the solve stopped short, and the line without the bit is a finished
   answer, every real root and the count of the complex ones, so a PARTIAL in its place would say less than the
   SOLVED it replaces.
3. **The order:** the real roots ascending, then the pairs by θ, minus first. A line 1.9.0 already prints whole under
   the bit (`X^3=1`, `X^4=16`, `X^6=64`, `X^8=16`, …) keeps its bytes, CAS 020's order included.
4. **Scope:** c a nonzero number, x^n = c only, n ≤ 12; letters as at 1.9.0; both models.
5. **Room: no card now.** Decision 94 lets the line rise to 301,056 B without one, and p1 leaves 4,129 B under it. The
   unit that needs more brings Michael its measured size first; the 043 notes name the room left.
6. **do_sum_ 816 → 832 (host):** hunted in the build; if it doesn't come out, a frame-table commit of its own naming
   the move and its cause, judged on the S3, where nothing moves.
- **The read-back at 1E-24 stays:** the forms are exact by construction, and the read-back is what refuses a slip; its
  mutant must go red.

**p1 with ruling 1's form:** of the 363 equations, 162 print every root (factored ρ fits more lines than p1's 123),
201 the line without the bit byte for byte, none anything else; every root of the 162 judged right outside casim; the
72 lines 1.9.0 prints whole under the bit are byte for byte 1.9.0's. By n in RAD: every c tried fits at n = 2, 3, 4
and 6; n = 5 at ±1; n = 8 at 1 and 16; n = 7 and 9 to 12 at none of them.

## As built

- **Where it runs:** cx_fix, after put_solution, from do_solve's one call site of CAS 041's pass: that call now goes to
  pp_pass, which takes cx_fix under the bit and pp_fix without it, so do_solve gains no branch. The line is built in the
  arena (SLOT bytes), not on the stack.
- **The form** (ruling 1): cx_put prints ρ×(A±B×0i1)÷D with A and B COS(θ) and SIN(θ) times their common denominator
  D, ρ and D left out where they are 1; a number ρ folds into A, B and D (X^4=-16 is SQRT(2)-SQRT(2)×0i1); where θ
  isn't the table's, ρ×(COS(θ)±SIN(θ)×0i1). X^3=1, X^4=16, X^6=64 and every line 1.9.0 prints whole under the bit
  keep their bytes (cx_fix leaves a SOLVED: line as it is).
- **The read-back** (ruling (b), kept): cx_check, (re + im i)^n in abn against c, each part within a relative 1E-24 of
  |c|. A miss falls to the floor, the line without the bit, never numeric solve's PARTIAL (p1 left PARTIAL there, which
  says less than the floor; found when the slip mutant was built by hand). cx043-slip plants a wrong angle for one pair
  (θ's denominator n+1 at k = 1): the read-back refuses it and X^3=2 prints SOLVED, 2 CPLX: X=2^(1÷3), so the whole lines
  go red; cx043-slip-unchecked plants the same with the read-back off, and X^3=2 prints SOLVED: X=2^(1÷3);
  X=-2^(1÷3)×0i1; X=2^(1÷3)×0i1, red by value on the core.
- **Every root or none:** cx_fix counts what it prints, the real roots and two a pair, and falls to the floor unless it
  is n. Without it, a slip that pushed θ to a half turn ended the pairs early and printed SOLVED: X=2^(1÷3) for X^3=2, a
  line that claims every root and has one (the first slip mutant, built by hand, found it); cx043-count-off goes red.
- **The steps** (abacus #17279, (a)): the PURE POWER page runs under the bit too (W->pure no longer excludes it), and
  do_ssteps, which already puts the answer on the ANSWER page, gives the page the answer's roots where the answer is
  every root (ss_cxpage): `PURE POWER: X^3=2: X=2^(1÷3); X=2^(1÷3)×(-1-SQRT(3)×0i1)÷2; X=…`. Elsewhere it is CAS
  041's page, as without the bit (`X^9=64: X=4^(1÷3)`, `X^7=1: NO OTHER REAL ROOT`). solve_test pins the page's roots
  to the answer's over the sweep (115 pages); cx043-page-off and cx043-page-roots go red. ss_walk's frame doesn't move.
- **CAS 028's boundary** (scripts/left-out-golden.py) judged each solve line's note by counting what the bit prints.
  Under CAS 043 the bit prints every root of a pure power, not factoring's pairs, so the 20 pure-power lines CAS 041
  moved are left to its boundary and to CAS 043's tests, and the script says how many.
- **Room** (ruling 5): S3 text 297,742 B, 3,314 B under decision 94's 301,056 B; the next unit that needs more brings
  Michael its measured size.
- **Frames** (ruling 6): on the S3 no accepted frame moves (build/s3's .su against 1.9.0's; cx_fix 464 B, the largest
  new). On the host do_sum_ 816 → 832 moves and was hunted: its own inlining decisions are unchanged; it moves under
  -fno-ipa-ra and -fno-ipa-sra alike; the new code's small() calls behind ir_rat, its helpers noinline and the pass
  marked cold each left it (cold moved two more); the nearest change found is small() newly inlined into
  sm_frac.constprop. A fourth shape's zero came from a splice that broke the source and was caught on a real build.
  The host table is re-recorded in a commit of its own (work/043-frames.txt).
- **The device:** DEVICE PASS, 1,516 cases byte-identical to the host at 16,384 B (scripts/measure.py's six CAS 043
  cases, the bench's first run under the complex bit, among them: device_cases.py ORs CASIM_COMPLEX into an angle written
  RAD+C), 298 steps pages; worst stack 11,360 B masked (STU chain X^ 44, derive), unchanged, + 384 = 11,744 of 12,288.
  CAS 043's cases reach 2,224 B (X^3=2 in DEG, and X^9=64's floor).
- **Mutants:** 13 (cx043-*, 12, and pp041-off re-anchored to pp_pass), all caught.
- **Tests:** make test 20 tests, 121,903 checks (with the host table re-recorded). solve_test's pure_complex: each
  ruling's text, model 33 as without the bit, and a sweep of n 2..12 over 14 values of c in three modes, 199 lines with
  every root (800 roots judged on the core, n distinct), 115 PURE POWER pages equal to their answers, 263 the line
  without the bit byte for byte.

## Today (1.9.0, RAD)

Under the bit, x^n = c prints what numeric solve's factors over Q give: CAS 020's pair for each quadratic factor, and
PARTIAL for any factor of degree 3 or more. CAS 041's pass (pp_fix), which gives the real roots of x^n = c, doesn't
run under the bit. So the bit prints less than its absence: X^3=2 is PARTIAL, DEGREE 3 LEFT where SOLVED, 2 CPLX:
X=2^(1÷3) without it; X^4=-16 is PARTIAL, DEGREE 4 LEFT where NO REAL ROOTS; X^5=32 is PARTIAL, DEGREE 4 LEFT: X=2
where SOLVED, 4 CPLX: X=2; and the same for X^4=2, X^6=-1, X^8=1, X^9=64, X^12=4096. Already whole under the bit:
X^2=-4, X^3=±1, X^3=8, X^4=1, X^4=16, X^6=1, X^6=64.

## What p1 does

1. **When:** a pass after put_solution, beside CAS 041's pp_fix and only under the bit, as pp_fix is only without it.
   A line already SOLVED with every root (CAS 020's) is left as it is. The typed text is read again by pp_read
   (x^n = c, n ≥ 3, c a nonzero number).
2. **The roots:** the real ones as pp_roots builds them (CAS 041 ruling 7). Each complex pair as ρ×COS(θ) ±
   ρ×SIN(θ)×0i1, ρ = |c|^(1÷n) built as pp_roots builds it, θ = (2k + [c < 0])÷n of a half turn, 0 < θ < 1, in the
   mode's unit (π, 180 or 200), canon's table reducing multiples of π/6 and π/4 (ruling (a)). The parts are two real
   trees; 0i1 is only printed.
3. **The check (ruling (b)):** each root read back in abn at 34 digits: (re + im i)^n against c, each part within a
   relative 1E-24 of |c|; a miss refuses the pass (the line stays as numeric solve left it).
4. **The line (ruling (c)):** SOLVED: then the real roots, then each pair, minus first, by θ. Where it doesn't fit
   191 B, the line without the bit (SOLVED, k CPLX with the real roots, or NO REAL ROOTS), which is the floor.

| equation (RAD) | p1 | bytes |
|---|---|---|
| X^3=2 | SOLVED: X=2^(1÷3); X=-2^(1÷3)÷2-2^(1÷3)×SQRT(3)÷2×0i1; X=-2^(1÷3)÷2+2^(1÷3)×SQRT(3)÷2×0i1 | 102 |
| X^4=-16 | SOLVED: X=SQRT(2)-SQRT(2)×0i1; X=SQRT(2)+SQRT(2)×0i1; X=-SQRT(2)-SQRT(2)×0i1; X=-SQRT(2)+SQRT(2)×0i1 | 104 |
| X^4=2 | SOLVED: X=-2^(1÷4); X=2^(1÷4); X=-2^(1÷4)×0i1; X=2^(1÷4)×0i1 | 66 |
| X^5=1 | SOLVED: X=1; X=COS(2×π÷5)-SIN(2×π÷5)×0i1; X=COS(2×π÷5)+SIN(2×π÷5)×0i1; X=COS(4×π÷5)-SIN(4×π÷5)×0i1; X=COS(4×π÷5)+SIN(4×π÷5)×0i1 | 155 |
| X^6=-1 | SOLVED: X=SQRT(3)÷2-1÷2×0i1; X=SQRT(3)÷2+1÷2×0i1; X=-0i1; X=0i1; X=-SQRT(3)÷2-1÷2×0i1; X=-SQRT(3)÷2+1÷2×0i1 | 119 |
| X^8=1 | SOLVED: X=-1; X=1; X=SQRT(2)÷2-SQRT(2)÷2×0i1; X=SQRT(2)÷2+SQRT(2)÷2×0i1; X=-0i1; X=0i1; X=-SQRT(2)÷2-SQRT(2)÷2×0i1; X=-SQRT(2)÷2+SQRT(2)÷2×0i1 | 154 |
| X^9=64 | SOLVED, 8 CPLX: X=4^(1÷3) (the floor: all nine don't fit) | |
| X^3=1 | SOLVED: X=1; X=(-1-SQRT(3)×0i1)÷2; X=(-1+SQRT(3)×0i1)÷2 (CAS 020's, as at 1.9.0) | 59 |

Measured over n = 2..12 and c in 1, -1, 2, -2, 8, -8, 16, 64, 1÷2, 3, -27, in RAD, DEG and GRAD (363 equations,
work/043-p1-measured.txt): 123 print every root, the other 240 print the line without the bit byte for byte, and none
prints anything else; every root of the 123 is judged right outside casim (z^n against c in Python's complex
arithmetic, n distinct roots). By n, every root fits for n = 2 and 3 at every c tried, for n = 4 at all but -8 and
-27, for n = 6 at 1, -1, 8, 64, 3 and -27, for n = 5 at ±1, and for n = 8 at 1 and 16; n = 7 and 9 to 12 fit at none
of these c, though X^12=4096 does (its twelve roots are integers and table values, 174 B).

**Cost:** S3 text 296,927 B (+3,202 on 1.9.0), 4,129 under 301,056. On the S3 no accepted frame moves (build/s3's .su
against 1.9.0's, by hand); cx_fix is 608 B, of which 192 is its line, which the build moves into the arena. On the host,
scripts/frame-table.py moves one: do_sum_ 816 → 832.

## For abacus to rule

1. **The form of a pair.** p1 distributes ρ: -2^(1÷3)÷2-2^(1÷3)×SQRT(3)÷2×0i1. Two others: ρ factored,
   2^(1÷3)×(-1-SQRT(3)×0i1)÷2, which is CAS 020's shape times ρ and shorter as ρ grows; or CAS 020's own form wherever
   cos and sin are in the table, and the cosine form only for the rest. Lean: ρ factored, (a±b×0i1) in canon's terms
   of the table's values, since it reads as CAS 020's roots do and fits more lines.
2. **Where the line doesn't fit.** p1 prints the line without the bit, complete about the real roots with their
   complex count (SOLVED, 8 CPLX: X=4^(1÷3)). The other reading of #17147: PARTIAL, DEGREE d LEFT with the real roots
   and as many pairs as fit. Lean: the line without the bit, since PARTIAL would say less than the SOLVED it replaces.
3. **The order:** real roots ascending, then each pair minus first, by θ. CAS 020 orders by canon. Lean: by θ, so
   a reader can see the roots go round.
4. **Scope:** c a number, n up to solve's 12, the pure power only (x^n = c, not (x+1)^n = c), and the lines numeric
   solve leaves PARTIAL or short of the floor. Letters (X^3=A under the bit) stay as at 1.9.0. Lean yes.
5. **Room:** S3 text 4,129 B under the line after p1. Michael's rule raises the room per unit on its measured size;
   say whether this unit's measurement goes to him now.
6. **The host frame do_sum_ 816 → 832:** the build hunts it first, as CAS 041's frames were hunted, before bringing
   the table.

## The oracle, proposed

Every x^n = c for n = 2..12 and c over small integers, fractions and their negatives, in three modes and both models:
under the bit, either every root (each judged on the core by value, n distinct, conjugates paired) or the line without
the bit byte for byte; never anything else. Every line 1.9.0 prints whole under the bit unchanged.
